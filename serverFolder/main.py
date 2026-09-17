"""
main.py -- AI Server trung tam (FastAPI).
Dieu phoi pipeline AI va dong bo du lieu he thong IoT:
  - POST /voice-query   : Nhan WAV tu ESP32 -> STT -> LLM -> TTS -> Tra WAV audio
  - POST /sensor-update : Nhan nhiet do/ap suat tu ESP8266 -> Cap nhat V2/V3
  - POST /sensor-error  : Nhan canh bao loi cam bien -> Hien thi V4
  - GET  /health        : Kiem tra trang thai server va du lieu cam bien gan nhat
"""
import asyncio
import datetime
import os
import sys
import traceback
import json
import requests

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

from fastapi import FastAPI, Request
from fastapi.responses import Response, JSONResponse
import uvicorn

import config
import blynk_client
import ai_pipeline

app = FastAPI()
os.makedirs(config.SAVE_DIR, exist_ok=True)
SENSOR_FILE = "sensor_data.json"

latest_sensor = {"temperature_c": None, "pressure_hpa": None, "received_at": None}
chat_history = []


def load_cached_sensor():
    """Doc du lieu cam bien tu file cache hoac Blynk Cloud de duy tri trang thai khi restart."""
    global latest_sensor
    # 1. Doc tu file cache cuc bo
    if os.path.exists(SENSOR_FILE):
        try:
            with open(SENSOR_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                latest_sensor.update(data)
                print(f"[Sensor] Da load tu cache file {SENSOR_FILE}: {latest_sensor}")
                return
        except Exception as e:
            print(f"[Sensor] Khong doc duoc {SENSOR_FILE}: {e}")

    # 2. Neu chua co, thu lay tu Blynk Cloud REST API (V2: Nhiet do, V3: Ap suat)
    try:
        t_url = f"{config.BLYNK_BASE}/get?token={config.BLYNK_AUTH_TOKEN}&V2"
        p_url = f"{config.BLYNK_BASE}/get?token={config.BLYNK_AUTH_TOKEN}&V3"
        t_resp = requests.get(t_url, timeout=4)
        p_resp = requests.get(p_url, timeout=4)
        if t_resp.status_code == 200 and p_resp.status_code == 200:
            t_val = float(t_resp.text.strip().replace('"', ''))
            p_val = float(p_resp.text.strip().replace('"', ''))
            latest_sensor["temperature_c"] = t_val
            latest_sensor["pressure_hpa"] = p_val
            latest_sensor["received_at"] = str(datetime.datetime.now().strftime("%H:%M:%S %d/%m/%Y"))
            save_cached_sensor()
            print(f"[Sensor] Da khoi phuc thanh cong tu Blynk Cloud: Nhiet do={t_val}C, Ap suat={p_val}hPa")
    except Exception as e:
        print(f"[Sensor] Khong lay duoc tu Blynk Cloud: {e}")


def save_cached_sensor():
    """Ghi du lieu cam bien xuong file de giu lai khi restart."""
    try:
        with open(SENSOR_FILE, "w", encoding="utf-8") as f:
            json.dump(latest_sensor, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"[Sensor] Loi ghi file cache: {e}")


load_cached_sensor()


@app.get("/health")
async def health():
    return {"status": "ok", "time": str(datetime.datetime.now()), "sensor": latest_sensor}


@app.post("/voice-query")
async def voice_query(request: Request):
    """Tiep nhan audio WAV tu ESP32, thuc thi STT -> LLM -> TTS va tra ve file am thanh dap thoai."""
    print("\n[/voice-query] Nhan yeu cau hoi thoai tu ESP32...")
    try:
        audio_bytes = await request.body()
        ts = datetime.datetime.now().strftime("%H%M%S")
        in_path = os.path.join(config.SAVE_DIR, f"tu_esp32_{ts}.wav")
        with open(in_path, "wb") as f:
            f.write(audio_bytes)
        print(f"[/voice-query] Da luu audio: {in_path} ({len(audio_bytes)} bytes)")

        # 1. Nhan dang giong noi (STT)
        t_stt_start = datetime.datetime.now()
        now_str = t_stt_start.strftime("%H:%M:%S")
        await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[{now_str}]PROCESSING_STT\n  ↓")
        text, lang, conf = await asyncio.to_thread(ai_pipeline.transcribe_audio, in_path)
        dur_stt = (datetime.datetime.now() - t_stt_start).total_seconds()
        print(f"[Whisper] ({dur_stt:.2f}s) '{text}' | {lang} ({conf:.0%})")

        if not text:
            now_str = datetime.datetime.now().strftime("%H:%M:%S")
            await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[{now_str}]IDLE (khong nghe ro)")
            reply_wav = await ai_pipeline.text_to_speech_wav(
                "Xin loi, toi khong nghe ro. Ban noi lai duoc khong?", ts=ts
            )
            return Response(content=reply_wav, media_type="audio/wav")

        # 2. Suy luan ngon ngu (LLM) ket hop ngu canh cam bien
        t_llm_start = datetime.datetime.now()
        now_str = t_llm_start.strftime("%H:%M:%S")
        await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[{now_str}]PROCESSING_LLM ({dur_stt:.2f}s)\n  ↓")
        answer = await asyncio.to_thread(ai_pipeline.ask_qwen, text, latest_sensor)
        dur_llm = (datetime.datetime.now() - t_llm_start).total_seconds()
        print(f"[Qwen] ({dur_llm:.2f}s) '{answer}'")

        # 3. Tong hop am thanh dap thoai (TTS)
        t_tts_start = datetime.datetime.now()
        now_str = t_tts_start.strftime("%H:%M:%S")
        await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[{now_str}]PROCESSING_TTS ({dur_llm:.2f}s)\n  ↓")
        reply_wav = await ai_pipeline.text_to_speech_wav(answer, ts=ts)
        dur_tts = (datetime.datetime.now() - t_tts_start).total_seconds()

        # 4. Cap nhat lich su hoi thoai len Blynk V5 (gioi han <= 1000 ky tu)
        conv_time = datetime.datetime.now().strftime("%H:%M:%S")
        entry = f"[{conv_time}]\nHỏi: {text}\nĐáp: {answer}"
        chat_history.append(entry)

        separator = "\n==============================\n"
        while len(chat_history) > 1 and len(separator.join(chat_history)) > 950:
            chat_history.pop(0)

        formatted_v5 = separator.join(chat_history)
        if len(formatted_v5) > 1000:
            formatted_v5 = formatted_v5[:997] + "..."

        await asyncio.to_thread(blynk_client.update, config.PIN_HISTORY, formatted_v5)

        total_ai_time = dur_stt + dur_llm + dur_tts
        print(f"[/voice-query] Hoan tat ({total_ai_time:.2f}s). Tra ve {len(reply_wav)} bytes cho ESP32.")
        
        headers = {
            "X-STT-Time": f"{dur_stt:.2f}",
            "X-LLM-Time": f"{dur_llm:.2f}",
            "X-TTS-Time": f"{dur_tts:.2f}",
            "X-Total-Time": f"{total_ai_time:.2f}"
        }
        return Response(content=reply_wav, media_type="audio/wav", headers=headers)

    except Exception:
        error_detail = traceback.format_exc()
        print("ERR DETAIL:\n", error_detail)
        now_str = datetime.datetime.now().strftime("%H:%M:%S")
        await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[{now_str}]IDLE (Loi AI Server)")
        return JSONResponse(status_code=500, content={"error": error_detail})


@app.post("/sensor-update")
async def sensor_update(request: Request):
    """Nhan du lieu nhiet do va ap suat tu Weather Node (ESP8266), relay len Blynk V2/V3."""
    data = await request.json()
    latest_sensor["temperature_c"] = data.get("temperature_c")
    latest_sensor["pressure_hpa"]  = data.get("pressure_hpa")
    latest_sensor["received_at"]   = str(datetime.datetime.now().strftime("%H:%M:%S %d/%m/%Y"))
    save_cached_sensor()
    print(f"[/sensor-update] Nhan du lieu cam bien: {latest_sensor}")

    if latest_sensor["temperature_c"] is not None:
        await asyncio.to_thread(blynk_client.update, config.PIN_TEMPERATURE, latest_sensor["temperature_c"])
    if latest_sensor["pressure_hpa"] is not None:
        await asyncio.to_thread(blynk_client.update, config.PIN_PRESSURE, latest_sensor["pressure_hpa"])

    return JSONResponse({"status": "ok", "received": latest_sensor})


@app.post("/sensor-error")
async def sensor_error(request: Request):
    """Nhan thong bao loi phan cung tu Weather Node va hien thi len terminal Blynk V4."""
    data = await request.json()
    msg = data.get("message", "Loi khong xac dinh")
    print(f"[/sensor-error] {msg}")
    await asyncio.to_thread(blynk_client.update, config.PIN_STATUS, f"[Weather] {msg}")
    return JSONResponse({"status": "ok"})


@app.get("/sensor-latest")
async def sensor_latest():
    return JSONResponse(latest_sensor)


@app.get("/system-status")
async def system_status():
    return JSONResponse({"running": True})


if __name__ == "__main__":
    blynk_client.start_background_thread()
    print("=" * 60)
    print(f"AI Server khoi chay tai {config.HOST}:{config.PORT}")
    print("=" * 60)
    uvicorn.run(app, host=config.HOST, port=config.PORT)

