import cv2
import os
import re

SAVE_DIR = "faces_owner"  # thư mục lưu ảnh chủ nhà
os.makedirs(SAVE_DIR, exist_ok=True)

# ===== TÍNH CHỈ SỐ ẢNH BẮT ĐẦU (ĐỂ KHÔNG GHI ĐÈ ẢNH CŨ) =====
pattern = re.compile(r"owner_(\d+)\.png")
max_idx = -1
for fname in os.listdir(SAVE_DIR):
    m = pattern.match(fname)
    if m:
        idx = int(m.group(1))
        if idx > max_idx:
            max_idx = idx

img_count = max_idx + 1
print(f"Thư mục {SAVE_DIR} hiện có {max_idx+1} ảnh, sẽ lưu từ index {img_count} trở đi.")

# ===== HAAR CASCADE PHÁT HIỆN MẶT =====
face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
)

cap = cv2.VideoCapture(0)
MAX_IMAGES_PER_SESSION = 200  # số ảnh muốn chụp thêm trong 1 lần chạy

print("Nhấn phím 's' để lưu ảnh mặt, 'q' để thoát.")

saved_this_session = 0

while True:
    ret, frame = cap.read()
    if not ret:
        print("Không đọc được camera")
        break

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # ===== DÒ MẶT: NHẠY HƠN =====
    # scaleFactor = 1.1 (nhạy hơn 1.3), minNeighbors = 3 (dễ bắt hơn 5)
    faces = face_cascade.detectMultiScale(gray, 1.1, 3)

    # Vẽ khung xanh to hơn quanh mặt
    if len(faces) > 0:
        (x, y, w, h) = faces[0]          # lấy mặt lớn nhất/đầu tiên
        pad = int(w * 0.35)              # tăng khung 35% mỗi bên
        x2 = max(0, x - pad)
        y2 = max(0, y - pad)
        x3 = min(gray.shape[1], x + w + pad)
        y3 = min(gray.shape[0], y + h + pad)

        cv2.rectangle(frame, (x2, y2), (x3, y3), (0, 255, 0), 2)

    cv2.putText(frame, "Nhan 's' de luu, 'q' de thoat",
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6,
                (0, 255, 255), 2)

    cv2.imshow("Capture faces", frame)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('s'):
        if len(faces) == 0:
            print("Khong thay mat ro, thu dua sat camera hon.")
            continue

        # dùng cùng khung to như khi vẽ để train/infer cho giống
        (x, y, w, h) = faces[0]
        pad = int(w * 0.35)
        x2 = max(0, x - pad)
        y2 = max(0, y - pad)
        x3 = min(gray.shape[1], x + w + pad)
        y3 = min(gray.shape[0], y + h + pad)

        face_img = gray[y2:y3, x2:x3]
        face_img = cv2.resize(face_img, (64, 64))

        filename = os.path.join(SAVE_DIR, f"owner_{img_count:03d}.png")
        cv2.imwrite(filename, face_img)
        img_count += 1
        saved_this_session += 1
        print(f"Đã lưu {filename}")

        if saved_this_session >= MAX_IMAGES_PER_SESSION:
            print("Đã đủ ảnh cho lần chụp này, thoát.")
            break

    elif key == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
