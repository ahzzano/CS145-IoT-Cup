from deepface import DeepFace

# try Facenet/ArcFace/RetinaFace more accurate slower just in case we need it
# try Sface for something less accurate but fast
MODEL_NAME = "Facenet" 
DETECTOR_BACKEND = "opencv"


def compare_faces(baseline_path: str, candidate_path: str) -> dict:
    result = DeepFace.verify(
        img1_path=baseline_path,
        img2_path=candidate_path,
        model_name=MODEL_NAME,
        detector_backend=DETECTOR_BACKEND,
    )

    distance = float(result["distance"])
    threshold = float(result["threshold"])
    confidence = max(0.0, min(100.0, (1.0 - distance / (2.0 * threshold)) * 100.0))

    return {
        "match": bool(result["verified"]),
        "confidence": round(confidence, 2),
    }
