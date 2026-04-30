import base64
import os
 
import cv2
import numpy as np
from dynaconf import Dynaconf
from flask import current_app
from flask_restx import Namespace, Resource, reqparse
from mosip_auth_sdk import MOSIPAuthenticator
from mosip_auth_sdk.models import DemographicsModel
 
from models.examinee import Examinee
from models.base import db
import utils

api = Namespace("mosip", description='all MOSIP related requests')

config = Dynaconf(settings_files=["./mosip_config.toml"], environments=False)
authenticator = MOSIPAuthenticator(config=config)

PHOTO_DIR = "serve/" # change this to where photos will be saved

# ── Parsers ──
auth_parser = reqparse.RequestParser()
auth_parser.add_argument("name", location="form", type=str, required=True)
auth_parser.add_argument("uin",  location="form", type=str, required=True)
 
kyc_parser = reqparse.RequestParser()
kyc_parser.add_argument("name", location="form", type=str, required=True)
kyc_parser.add_argument("uin",  location="form", type=str, required=True)

# ── POST /mosip/auth ──
@api.route("/auth")
class Auth(Resource):
    @api.expect(auth_parser)
    @api.doc(responses={
        200: "Returns auth_status: true/false",
        502: "MOSIP request failed",
    })
    def post(self):
        """Yes/no identity verification against MOSIP using name and UIN."""
        args = auth_parser.parse_args()
        name = args.get("name")
        uin  = args.get("uin")
 
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
 
        auth_status    = body.get("response", {}).get("authStatus", False)
        transaction_id = body.get("transactionID", "")
        errors         = body.get("errors")
 
        return utils.gen_success_message("auth complete", {
            "uin":            uin,
            "name":           name,
            "auth_status":    auth_status,
            "transaction_id": transaction_id,
            "errors":         errors,
        })

# ── POST /mosip/kyc ──
@api.route("/kyc")
class KYC(Resource):
    @api.expect(kyc_parser)
    @api.doc(responses={
        200: "Returns demographics and photo path",
        500: "Could not decode photo",
        502: "MOSIP request failed",
    })
    def post(self):
        """Fetch full KYC data from MOSIP and save the ID photo."""
        args = kyc_parser.parse_args()
        name = args.get("name")
        uin  = args.get("uin")
 
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
 