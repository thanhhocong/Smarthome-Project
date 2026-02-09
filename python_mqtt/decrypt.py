from Crypto.Cipher import AES
import base64
import json

# KHÓA AES PHẢI GIỐNG 100% VỚI KHÓA TRÊN ESP32
AES_KEY = bytes([
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10
])

def aes_decrypt_from_base64(cipher_b64: str) -> str:
    """
    Nhận chuỗi ciphertext dạng Base64 (chuỗi 'cipher' từ Core IoT),
    trả về chuỗi JSON gốc.
    """
    # 1. Base64 decode
    ciphertext = base64.b64decode(cipher_b64)

    # 2. AES-128-ECB decrypt
    cipher = AES.new(AES_KEY, AES.MODE_ECB)
    padded = cipher.decrypt(ciphertext)

    # 3. Bỏ padding PKCS#7
    pad_val = padded[-1]
    if pad_val <= 0 or pad_val > 16:
        raise ValueError("Sai padding, có thể key hoặc thuật toán không khớp.")
    plain_bytes = padded[:-pad_val]

    return plain_bytes.decode("utf-8")

if __name__ == "__main__":
    # Hỏi người dùng nhập chuỗi Base64 lấy từ Core IoT / Serial
    cipher_b64 = input("Nhập chuỗi cipher (Base64): ").strip()

    try:
        plain = aes_decrypt_from_base64(cipher_b64)
        print("\n===== PLAIN JSON =====")
        print(plain)

        # Thử parse JSON để lấy field
        data = json.loads(plain)
        print("\n===== GIÁ TRỊ TỪ JSON =====")
        print("Nhiệt độ:", data.get("temperature"))
        print("Độ ẩm:", data.get("humidity"))
        print("Pump_on:", data.get("pump_on"))
    except Exception as e:
        print("Lỗi giải mã:", e)
