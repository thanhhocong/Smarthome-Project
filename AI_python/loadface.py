import cv2
import numpy as np
import os
from sklearn.ensemble import IsolationForest
from joblib import dump

DATA_DIR = "faces_owner"
IMG_SIZE = (64, 64)

def load_face_vectors():
    X = []
    files = sorted(os.listdir(DATA_DIR))
    if not files:
        print("Thư mục faces_owner đang trống, hãy chạy facecap.py để chụp ảnh trước.")
        return np.empty((0, IMG_SIZE[0] * IMG_SIZE[1]), dtype="float32")

    for fname in files:
        path = os.path.join(DATA_DIR, fname)
        img = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
        if img is None:
            print("Không đọc được", path)
            continue

        img = cv2.resize(img, IMG_SIZE)
        vec = img.flatten().astype("float32") / 255.0  # chuẩn hóa 0–1
        X.append(vec)

    return np.array(X, dtype="float32")

print("Đang load dữ liệu mặt từ", DATA_DIR, "...")
X = load_face_vectors()

if X.shape[0] == 0:
    print("Không có mẫu nào, dừng lại.")
    exit(0)

print("Số mẫu:", X.shape)

# Train IsolationForest chỉ trên mặt của bạn
model = IsolationForest(
    n_estimators=100,
    contamination=0.05,  # tỷ lệ outlier dự kiến
    random_state=42
)
model.fit(X)

# Lưu model
dump(model, "face_isoforest.joblib")
print("Đã train xong và lưu model vào face_isoforest.joblib")
