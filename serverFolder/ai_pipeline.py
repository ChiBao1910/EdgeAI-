import os
import datetime
import io
import math
import struct
import wave
import requests
import edge_tts
from pydub import AudioSegment
from faster_whisper import WhisperModel
import config

print(f"[ai_pipeline] Tai model Whisper ({config.WHISPER_MODEL_SIZE})...")
_whisper_model = WhisperModel(config.WHISPER_MODEL_SIZE, device="cpu", compute_type="int8")
print("[ai_pipeline] Whisper da san sang.")


def transcribe_audio(wav_path: str):
    """Nhan dang giong noi (STT): Audio WAV -> (text, lang, prob)."""
    segments, info = _whisper_model.transcribe(wav_path, beam_size=5, language="vi")
    text = " ".join(s.text.strip() for s in segments).strip()
    return text, info.language, info.language_probability


def ask_qwen(text: str, sensor: dict) -> str:
    """Suy luan ngon ngu (LLM): Ket hop cau hoi voi du lieu cam bien moi nhat (Grounding)."""
    sensor_info = ""
    if sensor and sensor.get("temperature_c") is not None:
        t = sensor["temperature_c"]
        p = sensor.get("pressure_hpa")
        rec = sensor.get("received_at", "vừa xong")
        sensor_info = (
            f"\n\n[THÔNG TIN THỜI TIẾT & NHIỆT ĐỘ HIỆN TẠI ESP8266 đo được]:\n"
            f"- Nhiệt độ hiện tại: {t} độ C\n"
            f"- Áp suất khí quyển: {p} hPa\n"
            f"- Cập nhật lúc: {rec}\n\n"
            f"QUY TẮC BẮT BUỘC:\n"
            f"1. Khi người dùng hỏi về thời tiết, nhiệt độ hiện tại, hay môi trường, bạn PHẢI dùng trực tiếp số liệu trên ({t} độ C, {p} hPa) để trả lời ngắn gọn, tự nhiên.\n"
            f"2. Tuyệt đối KHÔNG được từ chối, KHÔNG nói thiếu thông tin, KHÔNG bảo người dùng tự đi tìm.\n"
        )
    else:
        sensor_info = "\n\n[Luu y: Hien chua ket noi duoc cam bien thoi tiet. Neu hoi thoi tiet, hay bao he thong dang ket noi cam bien.]"

    system_content = (
        "Ban la tro ly ao AI thong minh mang tên LEVA. "
        "Khi duoc hoi ban la ai thi tra loi la: Tôi là một mô hình trong lĩnh vực công nghệ thông tin. Tôi được thiết kế để đáp ứng yêu cầu và câu hỏi của người dùng bằng cách cung cấp thông tin chính xác, cập nhật và có tính khoa học cao. Tôi có thể giúp bạn với nhiều loại bài tập và các vấn đề khác nhau. "
        "Hay tra loi bang tieng Viet, rat ngan gon (1 den 2 cau), tu nhien, than thien de phat ra loa ro rang."
        + sensor_info
    )

    payload = {
        "model": config.QWEN_MODEL,
        "messages": [
            {"role": "system", "content": system_content},
            {"role": "user", "content": text},
        ],
        "stream": False,
    }
    response = requests.post(config.OLLAMA_URL, json=payload, timeout=120)
    response.raise_for_status()
    return response.json()["message"]["content"]


async def text_to_speech_wav(text: str, ts: str = None) -> bytes:
    """Tong hop giong noi (TTS): Text -> WAV PCM 16-bit 16kHz Stereo de phat qua I2S MAX98357A."""
    if not ts:
        ts = datetime.datetime.now().strftime("%H%M%S")

    os.makedirs(config.ANSWER_DIR, exist_ok=True)
    mp3_path = os.path.join(config.ANSWER_DIR, f"tra_loi_{ts}.mp3")

    communicate = edge_tts.Communicate(
        text=text,
        voice=config.TTS_VOICE,
        rate=config.TTS_RATE,
        volume=config.TTS_VOLUME,
        pitch=config.TTS_PITCH,
    )
    await communicate.save(mp3_path)

    # Chuyen doi sang dinh dang I2S ESP32 yeu cau: 16kHz, Stereo, 16-bit PCM
    audio = AudioSegment.from_mp3(mp3_path)
    audio = audio.normalize() + 4
    audio = audio.set_frame_rate(16000).set_channels(2).set_sample_width(2)
    pcm_data = audio.raw_data

    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(16000)
        wf.writeframes(pcm_data)

    return buf.getvalue()


def generate_beep_wav(freq=440, duration=0.6, sample_rate=16000) -> bytes:
    """Tao am thanh bip bao hieu khi gap su co."""
    n_samples = int(duration * sample_rate)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            val = int(10000 * math.sin(2 * math.pi * freq * i / sample_rate))
            frames += struct.pack("<h", val)
        wf.writeframes(bytes(frames))
    return buf.getvalue()

