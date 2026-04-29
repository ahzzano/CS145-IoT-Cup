import os
import sys
from deepface import DeepFace

PUBLIC_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "public") # remove this since we are querying from db
BASELINE_IMAGE_FILENAME = "uploaded_national_id.jpg" # Change this to query images from db
UPLOADED_IMAGE_FILENAME = "uploaded_image.jpg" # Change this to query images from db
MODEL_NAME = "SFace" # try Facenet more accurate slower just in case we need it
DETECTOR_BACKEND = "opencv"


def _resolve(name_or_path: str) -> str:
    if os.path.isabs(name_or_path) and os.path.exists(name_or_path):
        return name_or_path
    direct = os.path.join(PUBLIC_DIR, name_or_path)
    if os.path.exists(direct):
        return direct
    for ext in (".jpg", ".jpeg", ".png"):
        candidate = os.path.join(PUBLIC_DIR, name_or_path + ext)
        if os.path.exists(candidate):
            return candidate
    raise FileNotFoundError(f"Could not find image '{name_or_path}' in {PUBLIC_DIR}")


def compare_faces(baseline: str, candidate: str) -> dict:
    baseline_path = _resolve(baseline)
    candidate_path = _resolve(candidate)

    result = DeepFace.verify(
        img1_path=baseline_path,
        img2_path=candidate_path,
        model_name=MODEL_NAME,
        detector_backend=DETECTOR_BACKEND,
    )

    distance = float(result["distance"])
    threshold = float(result["threshold"])
    # Map distance to a 0-100 confidence: distance==0 -> 100, distance>=2*threshold -> 0.
    confidence = max(0.0, min(100.0, (1.0 - distance / (2.0 * threshold)) * 100.0))

    # threshold is the threshold value used to determine if the match is valid.
    # confidence is the confidence score of the match (0-100)
    # distance is the raw similarity score between the two face embeddings.
    return {
        "match": bool(result["verified"]),
        "confidence": round(confidence, 2),
        # "distance": round(distance, 4),
        # "threshold": round(threshold, 4),
        # "model": MODEL_NAME,
    }


if __name__ == "__main__":
    candidate = sys.argv[1] if len(sys.argv) > 1 else UPLOADED_IMAGE_FILENAME
    baseline = sys.argv[2] if len(sys.argv) > 2 else BASELINE_IMAGE_FILENAME
    print(compare_faces(baseline, candidate))
