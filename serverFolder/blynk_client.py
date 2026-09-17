"""
blynk_client.py -- Dong bo du lieu len Blynk Cloud qua REST API (Thread-safe, Rate-limited).
"""
import threading
import time
import requests
import config

_lock = threading.Lock()
_last_call = 0.0


def update(pin: str, value) -> bool:
    """Ghi gia tri len Blynk Virtual Pin qua REST API voi co che Rate Limiter va Lock dong bo."""
    global _last_call
    with _lock:
        wait = config.BLYNK_MIN_INTERVAL - (time.time() - _last_call)
        if wait > 0:
            time.sleep(wait)
        try:
            pin_upper = pin.upper()
            # Blynk Datastream gioi han do dai chuoi duoi 1024 ky tu
            if isinstance(value, str) and len(value) > 1000:
                value = value[:997] + "..."
            resp = requests.get(
                f"{config.BLYNK_BASE}/update",
                params={"token": config.BLYNK_AUTH_TOKEN, pin_upper: value},
                timeout=5,
            )
            _last_call = time.time()
            ok = resp.status_code == 200
            print(f"[Blynk] Ghi {pin}={value!r} -> HTTP {resp.status_code}"
                  + ("" if ok else f" | {resp.text}"))
            return ok
        except Exception as e:
            print(f"[Blynk] Loi ghi {pin}: {e}")
            return False


def start_background_thread() -> None:
    """Khoi dong thread nen quan ly ket noi Blynk (neu can)."""
    pass
