from datetime import datetime
import importlib.util
import os
import tempfile
from pathlib import Path

from flask import current_app
from flask_restx.errors import HTTPStatus
from sqlalchemy import func
from werkzeug.datastructures import FileStorage

from flask_restx import Namespace, Resource, abort, reqparse

from auth import auth_required
from logs import on_examinee_creation
from models.exam_kit import ExamKit
from models.examinee import *
from models.examinee_picture import ExamineePicture
from models.logs import LogEntry

import utils

api = Namespace("examinee", description='All API endpoints for Examinees')


def _compare_face_bytes(baseline: bytes, candidate: bytes) -> dict:
    module_path = Path(__file__).resolve().parents[1] / "face-recognition" / "compare_images.py"
    spec = importlib.util.spec_from_file_location("compare_images", module_path)
    compare_images = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compare_images)

    with tempfile.NamedTemporaryFile(suffix=".jpg") as base_f, \
         tempfile.NamedTemporaryFile(suffix=".jpg") as cand_f:
        base_f.write(baseline); base_f.flush()
        cand_f.write(candidate); cand_f.flush()
        return compare_images.compare_faces(base_f.name, cand_f.name)


def _read_examinee_baseline(examinee) -> bytes:
    pic_path = os.path.join(current_app.config['UPLOAD_FOLDER'], examinee.picture)
    with open(pic_path, 'rb') as f:
        return f.read()


def _get_examinee_or_error(examinee_id: int):
    examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
    if examinee is None:
        return None, utils.gen_error("Examinee does not exist", 404)
    return examinee[0], None


def _get_authenticated_examinee_or_error(user):
    try:
        examinee_id = int(user["uin"])
    except (KeyError, TypeError, ValueError):
        return None, None, utils.gen_error("Invalid MOSIP auth token", 401)

    examinee, error = _get_examinee_or_error(examinee_id)
    return examinee_id, examinee, error


def _latest_picture_session(examinee_id: int):
    return (
        db.session.query(ExamineePicture)
        .filter_by(examinee_id=examinee_id)
        .order_by(ExamineePicture.id.desc())
        .first()
    )


def _latest_log_entry(examinee_id: int):
    return (
        db.session.query(LogEntry)
        .filter_by(examinee=examinee_id)
        .order_by(LogEntry.log_id.desc())
        .first()
    )

examinee_parser = reqparse.RequestParser()
examinee_parser.add_argument('id', type=int, help='id of the user', location='args')

examinee_parser_creator = reqparse.RequestParser()
examinee_parser_creator.add_argument('file', location='files', type=FileStorage, required=True)
examinee_parser_creator.add_argument('name', location='form', type=str, required=True)
examinee_parser_creator.add_argument('id', type=int,location='form')

@api.route('/')
class GetExaminee(Resource):
    @api.expect(examinee_parser)
    @api.doc(responses={400: "Missing ID Parameter", 404:"Examinee does not exist", 200: "Returns examinee"})
    def get(self):
        args = examinee_parser.parse_args()

        id = args.get('id')
        if id is None:
            return utils.gen_error("Missing ID Parameter", 400)

        user = db.session.execute(db.select(Examinee).filter_by(id=id)).first()
        if user is None:
            return utils.gen_error("Examinee does not exist", 404)
        return utils.gen_success_message("returned examinee", user[0].to_dict())
    
    @api.expect(examinee_parser_creator)
    @api.doc(responses={400: "Missing parameters", 200: "Returns new user"})
    def post(self):
        args = examinee_parser_creator.parse_args()
        file = args.get('file')
        if not file:
            return utils.gen_error("No file provided", 400)

        name = args.get('name')
        if not name:
            return utils.gen_error("No name provided", 400)

        requested_id = args.get('id')
        if requested_id:
            existing = db.session.execute(db.select(Examinee).filter_by(id=requested_id)).first()
            if existing:
                return utils.gen_success_message("new user created", existing[0].to_dict())

        filename = file.filename
        file.save(os.path.join(current_app.config['UPLOAD_FOLDER'], filename))

        examinee = Examinee(name, filename)
        if requested_id:
            examinee.id = requested_id

        examinee.name = name
        examinee.picture = filename

        db.session.add(examinee)

        ek = ExamKit(False, None)
        db.session.add(ek)
        db.session.flush()

        if not db.session.query(LogEntry).filter_by(examinee=examinee.id).first():
            le = LogEntry(examinee.id)
            db.session.add(le)

        on_examinee_creation(examinee)

        db.session.commit()

        # print(f"[/examinee/] created examinee_id={examinee.id} name={examinee.name} picture={examinee.picture}")

        return utils.gen_success_message("new user created", examinee.to_dict())
    
    @api.expect(examinee_parser)
    @api.doc(responses={400: "Missing ID Parameter", 404:"Examinee does not exist", 200: "Returns success"})
    def delete(self):
        args = examinee_parser.parse_args()
        id = args.get('id')
        if id is None:
            return utils.gen_error("Missing ID", 400)
            # abort(HTTPStatus.BAD_REQUEST, "Missing ID Parameter")

        examinee = db.session.execute(db.select(Examinee).filter_by(id=id)).first()
        db.session.commit()

        if examinee is None:
            return utils.gen_error("Examinee does not exist", 404)
        
        picture_file = examinee[0].picture
        pic_path = os.path.join(current_app.config['UPLOAD_FOLDER'], picture_file)
        if os.path.exists(pic_path):
            os.remove(pic_path)

        db.session.delete(examinee[0])

        ek = db.session.query(ExamKit).order_by(ExamKit.kit_id.desc()).first()
        if not ek is None:
            db.session.delete(ek)

        db.session.commit()
        return utils.gen_success_message("deleted user", None)

logtime_args = reqparse.RequestParser()
logtime_args.add_argument('examinee_id', location='form', type=int, required=True)
@api.route('/timein')
class LogTimeIn(Resource):
    @api.expect(logtime_args)
    @api.doc(responses={404:"examinee does not exist", 200: "returns success"})
    def post(self):
        args = logtime_args.parse_args()

        examinee_id = args.get('examinee_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        db.session.commit()

        if examinee is None:
            return utils.gen_error("Examinee does not exist", 404)
        
        log_entry = db.session.query(LogEntry).filter_by(examinee=examinee_id).order_by(LogEntry.log_id.desc()).first()

        if log_entry:
            log_entry.exam_time_in = datetime.now()

        db.session.commit()

        return utils.gen_success_message("examinee has timed in", 200)

timeout_args = reqparse.RequestParser()
timeout_args.add_argument('examinee_id', location='form', type=int, required=True)
timeout_args.add_argument('exam_kit_id', type=int,location='form', required=True)
@api.route('/timeout')
class LogTimeOut(Resource):
    @api.expect(timeout_args)
    @api.doc(responses={404:"Examinee or Exam kit does not exist", 200: "returns success"})
    def post(self):
        args = timeout_args.parse_args()

        examinee_id = args.get('examinee_id')
        exam_kit_id = args.get('exam_kit_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        exam_kit = db.session.execute(db.select(ExamKit).filter_by(kit_id=exam_kit_id)).first()
        db.session.commit()

        if examinee is None:
            return utils.gen_error("Examinee does not exist", 404)
        
        if exam_kit is None:
            return utils.gen_error("Exam kit does not exist", 404)

        if examinee[0].id != exam_kit[0].examinee_id:
            return utils.gen_error("Exam kit is not linked with examinee", 400)
        
        log_entry = db.session.query(LogEntry).filter_by(examinee=examinee_id).order_by(LogEntry.log_id.desc()).first()

        if log_entry:
            if log_entry.exam_time_in is None:
                return utils.gen_error("Examinee has not timed in", 400)

            log_entry.exam_time_out = datetime.now()

        db.session.commit()

        return utils.gen_success_message("examinee has timed out", 200)


pretest_parser = reqparse.RequestParser()
pretest_parser.add_argument('file', location='files', type=FileStorage, required=True)

@api.route('/pretest')
class PreTestFace(Resource):
    method_decorators = [auth_required]

    @api.expect(pretest_parser)
    @api.doc(responses={
            400: "Missing image or examinee baseline",
            401: "Missing or invalid MOSIP auth",
            403: "Face mismatch",
            404: "Examinee does not exist",
            200: "Face match"
        })
    def post(self, user):
        args = pretest_parser.parse_args()
        examinee_id, examinee, error = _get_authenticated_examinee_or_error(user)
        if error:
            # print(f"[/pretest] examinee_id={examinee_id} not found")
            return error

        pre_test_bytes = args.get('file').read()
        if not pre_test_bytes:
            return utils.gen_error("No pre-test image provided", 400)

        try:
            baseline_bytes = _read_examinee_baseline(examinee)
        except FileNotFoundError:
            # print(f"[/pretest] examinee_id={examinee_id} baseline picture missing on disk: {examinee.picture}")
            return utils.gen_error("Examinee baseline picture not found on disk", 400)

        try:
            comparison = _compare_face_bytes(baseline_bytes, pre_test_bytes)
        except Exception as exc:
            # print(f"[/pretest] examinee_id={examinee_id} compare error: {exc}")
            return {
                "error": "Face comparison failed",
                "data": {
                    "allowed": False,
                    "pre_conf": None,
                    "compare_error": str(exc),
                },
            }, 400

        picture = _latest_picture_session(examinee.id)
        if picture is None:
            picture = ExamineePicture(
                pre_test=pre_test_bytes,
                examinee_id=examinee.id,
                post_test=b'',
                post_conf=0.0,
            )
        else:
            picture.pre_test = pre_test_bytes
            picture.post_test = b''
            picture.post_conf = 0.0

        picture.pre_conf = comparison["confidence"]
        db.session.add(picture)

        log_entry = _latest_log_entry(examinee.id)
        if log_entry:
            log_entry.exam_time_in = datetime.now()

        db.session.commit()

        allowed = comparison["match"]
        pre_conf = comparison["confidence"]
        # print(f"[/pretest] examinee_id={examinee_id} picture_id={picture.id} match={allowed} pre_conf={pre_conf}")

        return {
            **({"error": "Faces don't match"} if not allowed else {}),
            "message": "ok" if allowed else "not_allowed",
            "data": {
                "allowed": allowed,
                "pre_conf": pre_conf,
                "min_confidence": comparison["min_confidence"],
                "distance": comparison["distance"],
                "threshold": comparison["threshold"],
                "deepface_verified": comparison["deepface_verified"],
                "model": comparison["model"],
                "detector_backend": comparison["detector_backend"],
                "exam_time_in": log_entry.exam_time_in.isoformat() if log_entry and log_entry.exam_time_in else None,
                "picture": picture.to_dict(),
            },
        }, 200 if allowed else 403


posttest_parser = reqparse.RequestParser()
posttest_parser.add_argument('file', location='files', type=FileStorage, required=True)


@api.route('/posttest')
class PostTestFace(Resource):
    method_decorators = [auth_required]

    @api.expect(posttest_parser)
    @api.doc(responses={
        400: "Missing image or no pre-test row",
        401: "Missing or invalid MOSIP auth",
        403: "Face mismatch",
        404: "Examinee does not exist",
        200: "Face match"
    })
    def post(self, user):
        args = posttest_parser.parse_args()
        examinee_id, examinee, error = _get_authenticated_examinee_or_error(user)
        if error:
            # print(f"[/posttest] examinee_id={examinee_id} not found")
            return error

        picture = _latest_picture_session(examinee.id)
        if picture is None:
            # print(f"[/posttest] examinee_id={examinee_id} has no pre_test row")
            return utils.gen_error("No pre-test session found for examinee", 400)

        post_test_bytes = args.get('file').read()
        if not post_test_bytes:
            return utils.gen_error("No post-test image provided", 400)

        try:
            baseline_bytes = _read_examinee_baseline(examinee)
        except FileNotFoundError:
            # print(f"[/posttest] examinee_id={examinee_id} baseline picture missing on disk: {examinee.picture}")
            return utils.gen_error("Examinee baseline picture not found on disk", 400)

        try:
            comparison = _compare_face_bytes(baseline_bytes, post_test_bytes)
        except Exception as exc:
            # print(f"[/posttest] examinee_id={examinee_id} compare error: {exc}")
            return {
                "error": "Face comparison failed",
                "data": {
                    "allowed": False,
                    "post_conf": None,
                    "compare_error": str(exc),
                },
            }, 400

        picture.post_test = post_test_bytes
        picture.post_conf = comparison["confidence"]

        log_entry = _latest_log_entry(examinee.id)
        if log_entry:
            log_entry.exam_time_out = datetime.now()

        db.session.commit()

        allowed = comparison["match"]
        post_conf = comparison["confidence"]
        # print(f"[/posttest] examinee_id={examinee_id} picture_id={picture.id} match={allowed} post_conf={post_conf}")

        return {
            **({"error": "Faces don't match"} if not allowed else {}),
            "message": "ok" if allowed else "not_allowed",
            "data": {
                "allowed": allowed,
                "post_conf": post_conf,
                "min_confidence": comparison["min_confidence"],
                "distance": comparison["distance"],
                "threshold": comparison["threshold"],
                "deepface_verified": comparison["deepface_verified"],
                "model": comparison["model"],
                "detector_backend": comparison["detector_backend"],
                "exam_time_out": log_entry.exam_time_out.isoformat() if log_entry and log_entry.exam_time_out else None,
                "picture": picture.to_dict(),
            },
        }, 200 if allowed else 403
