from datetime import datetime
import importlib.util
import io
import os
import tempfile
from pathlib import Path

from flask import current_app, request
from flask_restx.errors import HTTPStatus
from sqlalchemy import func
from werkzeug.datastructures import FileStorage

from flask_restx import Namespace, Resource, abort, reqparse

from auth import auth_required, bypassed
from logs import on_examinee_creation
from models.exam_kit import ExamKit
from models.examinee import *
from models.examinee_picture import ExamineePicture
from models.logs import LogEntry

from facerec import compare_images
import numpy as np
from PIL import Image

import queue

import utils

api = Namespace("examinee", description='All API endpoints for Examinees')

def compare_faces(a: bytes, b: bytes) -> dict: 
    a_img_pillow = np.array(Image.open(io.BytesIO(a)).convert("RGB"))
    b_img_pillow = np.array(Image.open(io.BytesIO(b)).convert("RGB"))

    return compare_images.compare_faces_2(a_img_pillow, b_img_pillow)

def decode_input_image(args) -> bytes | None:
    hex_data = args.get('hex_data')
    file     = args.get('file')
    raw_image = args.get('raw_image')

    if hex_data:
        try:
            cleaned = ''.join(hex_data.split())
            return bytes.fromhex(cleaned)
        except ValueError:
            return None

    if file:
        return file.read() or None

    if raw_image:
        return raw_image

    return None

def _get_examinee_or_error(examinee_id: int):
    examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
    if examinee is None:
        return None, utils.gen_error("Examinee does not exist", 404)
    return examinee[0], None

def _get_authenticated_examinee_or_error(uin: int):
    examinee, error = _get_examinee_or_error(uin)
    return examinee, error

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

enrolled_parser = reqparse.RequestParser()
enrolled_parser.add_argument('uin', location='args', type=int, required=True)

task_queue = queue.Queue()

@api.route('/enrolled/')
class AddExamineeQueue(Resource):
    @api.expect(enrolled_parser)
    def get(self):
        args = enrolled_parser.parse_args()

        id = args.get('uin')
        if not id:
            return utils.gen_error("No UIN provided", 400)

        user = db.session.execute(db.select(Examinee).filter_by(id=id)).first()

        if user is None:
            return utils.gen_error("Examinee does not exist", 403)
        
        task_queue.put(id)

        return utils.gen_success_message("Examinee exists. Adding to queue", {})

@api.route('/clear_queue/')
class ClearExamineeQueue(Resource):
    def get(self):
        print('Clearing Queue')
        while not task_queue.empty():
            item = task_queue.get()
            print(f'Item: {item}')

        return utils.gen_success_message("Cleared queue", {})

examinee_parser_creator = reqparse.RequestParser()
examinee_parser_creator.add_argument('file', location='files', type=FileStorage, required=True)
@api.route('/')
class GetExaminee(Resource):
    method_decorators = [auth_required]

    @api.doc(responses={400: "Missing ID Parameter", 404:"Examinee does not exist", 200: "Returns examinee"})
    def get(self, user):
        id = int(user['uin'])

        user = db.session.execute(db.select(Examinee).filter_by(id=id)).first()
        if user is None:
            return utils.gen_error("Examinee does not exist", 404)
        return utils.gen_success_message("Returned examinee", user[0].to_dict())
    
    @api.expect(examinee_parser_creator)
    @api.doc(responses={400: "Missing parameters", 200: "Returns new user"})
    def post(self, user):
        args = examinee_parser_creator.parse_args()
        file = args.get('file')
        if not file:
            return utils.gen_error("No file provided", 400)

        name = user['name']

        requested_id = int(user['uin'])
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
    
    @api.doc(responses={400: "Missing ID Parameter", 404:"Examinee does not exist", 200: "Returns success"})
    def delete(self, user):
        id = int(user['uin'])

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

pretest_parser = reqparse.RequestParser()
pretest_parser.add_argument('file',     location='files', type=FileStorage, required=False)
pretest_parser.add_argument('hex_data', location='form',  type=str,         required=False)
@api.route('/timein')
class PreTestFace(Resource):
    @api.expect(pretest_parser)
    @api.doc(responses={
            400: "Missing image or examinee baseline",
            401: "Missing or invalid MOSIP auth",
            403: "Face mismatch",
            404: "Examinee does not exist",
            200: "Face match"
        })
    def post(self):
        args = pretest_parser.parse_args()
        if request.mimetype == 'image/jpeg':
            args['raw_image'] = request.get_data() or None
        if task_queue.empty():  
            return utils.gen_error("No examinee in queue", 400)
        examinee_id = int(task_queue.get())
        examinee, error = _get_authenticated_examinee_or_error(examinee_id)
        if error:
            task_queue.put(examinee_id)
            return error
        pre_test_bytes = decode_input_image(args)
        if not pre_test_bytes:
            task_queue.put(examinee_id)
            return utils.gen_error("No pre-test image provided", 400)

        ## REPLACE STARTS HERE
        picture_path = os.path.join(current_app.config['UPLOAD_FOLDER'], examinee.picture)
        if not os.path.exists(picture_path):
            task_queue.put(examinee_id)
            return utils.gen_error("ID picture does not exist", 400)

        with open(picture_path, "rb") as f:
            baseline_bytes = f.read()

        try:
            comparison = compare_faces(baseline_bytes, pre_test_bytes)
        except Exception as exc:
            task_queue.put(examinee_id)
            return utils.gen_error("Face comparison failed", {
                "allowed": False,
                "pre_conf": None,
                "compare_error": str(exc),
            })

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

        ## END REPLACE HERE
        log_entry = _latest_log_entry(examinee.id)
        if log_entry:
            log_entry.exam_time_in = datetime.now()

        db.session.commit()

        allowed = comparison["match"]
        pre_conf = comparison["confidence"]

        if not allowed:
            task_queue.put(examinee_id)
            return utils.gen_error("Faces do not match", 403)

        task_queue.put(examinee_id)
        return utils.gen_success_message("Timein Success", {})

@api.route('/timeout')
class PostTestFace(Resource):
    @api.expect(pretest_parser)
    @api.doc(responses={
        400: "Missing image or no pre-test row",
        401: "Missing or invalid MOSIP auth",
        403: "Face mismatch",
        404: "Examinee does not exist",
        200: "Face match"
    })
    def post(self):
        args = pretest_parser.parse_args()
        if request.mimetype == 'image/jpeg':
            args['raw_image'] = request.get_data() or None
        if task_queue.empty():
            return utils.gen_error("No examinee in queue", 400)
        examinee_id = int(task_queue.get())
        examinee, error = _get_authenticated_examinee_or_error(examinee_id)

        if error:
            task_queue.put(examinee_id)
            return error

        picture = _latest_picture_session(examinee.id)
        if picture is None:
            task_queue.put(examinee_id)
            return utils.gen_error("No pre-test session found for examinee", 400)

        post_test_bytes = decode_input_image(args)
        if not post_test_bytes:
            task_queue.put(examinee_id)
            return utils.gen_error("No post-test image provided", 400)

        pre_test_bytes = picture.pre_test
        if not pre_test_bytes:
            task_queue.put(examinee_id)
            return utils.gen_error("No pre-test image found for examinee", 400)

        comparison = compare_faces(pre_test_bytes, post_test_bytes)

        picture.post_test = post_test_bytes
        picture.post_conf = comparison["confidence"]

        log_entry = _latest_log_entry(examinee_id)
        if log_entry:
            log_entry.exam_time_out = datetime.now()

        db.session.commit()

        allowed = comparison["match"]

        if not allowed:
            task_queue.put(examinee_id)
            return utils.gen_error("Faces do not match", 403)

        task_queue.put(examinee_id)
        return utils.gen_success_message("Timeout Success", {})
