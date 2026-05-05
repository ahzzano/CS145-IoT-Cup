import base64
import os
import json
import cv2
import numpy as np
from dynaconf import Dynaconf
from flask import current_app, make_response
from flask_restx import Namespace, Resource, reqparse
from mosip_auth_sdk import MOSIPAuthenticator
from mosip_auth_sdk.models import DemographicsModel
from werkzeug.datastructures import FileStorage
from auth import auth_required, generate_jwt
from models.examinee import Examinee
from models.base import db
import utils

api = Namespace("mosip", description="All MOSIP related requests")

config = Dynaconf(settings_files=["./mosip_config.toml"], environments=False)
authenticator = MOSIPAuthenticator(config=config)

PHOTO_DIR = "serve/" # change this to where photos will be saved

# ── Shared helper ───
def read_qr(file: FileStorage) -> str | None:
    """Decode a QR code from an uploaded image. Returns the raw string or None."""
    img_bytes = np.frombuffer(file.read(), dtype=np.uint8)
    img = cv2.imdecode(img_bytes, cv2.IMREAD_COLOR)
    if img is None:
        return None
    data, _, _ = cv2.QRCodeDetector().detectAndDecode(img)
    return data if data else None

def parse_national_id_qr(qr_data: str) -> tuple[str, str] | tuple[None, None]:
    """
    Parse the decoded QR string from a Philippine National ID.
    Returns (uin, name) or (None, None) if parsing fails.
    """
    try:
        payload = json.loads(qr_data)
        uin  = payload["uin"]
        name = payload["name"]
        return uin, name
    except (IndexError, AttributeError):
        return None, None

# ── Parsers ──
qr_parser = reqparse.RequestParser()
qr_parser.add_argument(
    "file", location="files", type=FileStorage, required=True,
    help="Image of the National ID QR code"
)

# ── POST /mosip/auth ──
@api.route("/auth")
class Auth(Resource):
    @api.expect(qr_parser)
    @api.doc(responses={
        200: "Returns auth_status: true/false",
        400: "No QR code found or could not parse ID",
        502: "MOSIP request failed",
    })
    def post(self):
        """Yes/no identity verification — scans QR code from uploaded National ID image."""
        args = qr_parser.parse_args()

        qr_data = read_qr(args.get("file"))
        if not qr_data:
            return utils.gen_error("No QR code found in image", 400)

        uin, name = parse_national_id_qr(qr_data)
        if not uin or not name:
            return utils.gen_error("Could not parse UIN and name from QR code", 400)

        demographics_data = DemographicsModel(
            name=[{"language": "eng", "value": name}],
        )
 
        response = authenticator.auth(
            individual_id=uin,
            individual_id_type="UIN",
            demographic_data=demographics_data,
            consent=True,
        )

        if not response.ok:
            return utils.gen_error("MOSIP auth request failed", 502)
 
        body = response.json()
 
        auth_status     = body.get("response", {}).get("authStatus", False)
        transaction_id  = body.get("transactionID", "")
        errors          = body.get("errors")
        jwt_token       = generate_jwt({'uin': uin, 'name': name})

        success_response = make_response(utils.gen_success_message("auth complete", {
            "uin":            uin,
            "name":           name,
            "auth_status":    auth_status,
            "transaction_id": transaction_id,
            # "token":            jwt_token,
            "errors":         errors,
        }))
        print(jwt_token)
        success_response.set_cookie(
            'token',
            jwt_token,
            samesite='Lax'
        )

        return  success_response

# ── POST /mosip/kyc ──
@api.route("/kyc")
class KYC(Resource):
    method_decorators = [auth_required]
    @api.doc(responses={
        200: "Returns demographics and photo path",
        400: "No QR code found or could not parse ID",
        500: "No photo found or could not decode photo",
        502: "MOSIP request failed",
    })
    def get(self, user):
        """Fetch full KYC data and save ID photo — scans QR code from uploaded National ID image."""
        uin: str = user['uin']
        name: str = user['name']

        demographics_data = DemographicsModel(
            name=[{"language": "eng", "value": name}],
        )
 
        response = authenticator.kyc(
            individual_id=uin,
            individual_id_type="UIN",
            demographic_data=demographics_data,
            consent=True,
        )
 
        if not response.ok:
            return utils.gen_error("MOSIP KYC request failed", 502)
 
        body      = response.json()
        decrypted = authenticator.decrypt_response(body)
        face_bytes = base64.b64decode(decrypted.pop("photo"))
 
        # decode image
        img = None
        for offset in range(70, 85):
            arr = np.frombuffer(face_bytes[offset:], dtype=np.uint8)
            img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
            if img is not None:
                break
 
        if img is None:
            return utils.gen_error("Could not decode photo from MOSIP response", 500)
 
        # save photo keyed by UIN so it can be retrieved later
        os.makedirs(PHOTO_DIR, exist_ok=True)
        photo_filename = f"{uin}.jpg"
        photo_path = os.path.join(PHOTO_DIR, photo_filename)
        cv2.imwrite(photo_path, img)
 
        return utils.gen_success_message("kyc complete", {
            "uin":          uin,
            "name":         name,
            "demographics": decrypted,
            "photo_path":   photo_path,
        })

@api.route("/auth2_test")
class Auth2Test(Resource):
    method_decorators = [auth_required]
    def get(self, user):
        return utils.gen_success_message("Enjoy your evening", {})
