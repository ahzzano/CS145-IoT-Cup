from deepface import DeepFace

# try Facenet/ArcFace/RetinaFace more accurate slower just in case we need it
# try Sface for something less accurate but fast
MODEL_NAME = "Facenet512" 
DETECTOR_BACKEND = "opencv"
ENFORCE_DETECTION = False
MIN_CONFIDENCE = 50.0


def compare_faces(baseline_path: str, candidate_path: str) -> dict:
    result = DeepFace.verify(
        img1_path=baseline_path,
        img2_path=candidate_path,
        model_name=MODEL_NAME,
        detector_backend=DETECTOR_BACKEND,
        enforce_detection=ENFORCE_DETECTION,
    )

    distance = float(result["distance"])
    threshold = float(result["threshold"])
    confidence = max(0.0, min(100.0, (1.0 - distance / (2.0 * threshold)) * 100.0))

    return {
        "match": confidence >= MIN_CONFIDENCE,
        "confidence": round(confidence, 2),
        "min_confidence": MIN_CONFIDENCE,
        "distance": round(distance, 6),
        "threshold": round(threshold, 6),
        "deepface_verified": bool(result["verified"]),
        "model": MODEL_NAME,
        "detector_backend": DETECTOR_BACKEND,
        "enforce_detection": ENFORCE_DETECTION,
    }
