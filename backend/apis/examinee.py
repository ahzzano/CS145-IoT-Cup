from datetime import datetime
import os

from flask import current_app
from flask_restx.errors import HTTPStatus
from sqlalchemy import func
from werkzeug.datastructures import FileStorage

from flask_restx import Namespace, Resource, abort, reqparse

from logs import on_examinee_creation
from models.exam_kit import ExamKit
from models.examinee import *
from models.logs import LogEntry

import utils

api = Namespace("examinee", description='All API endpoints for Examinees')

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
            return {"errror": "missing ID Parameter"}, 400

        user = db.session.execute(db.select(Examinee).filter_by(id=id)).first()
        if user is None:
            return {"error": "examinee does not exist"}, 404
        return utils.gen_success_message("returned examinee", user[0].to_dict())
    
    @api.expect(examinee_parser_creator)
    def post(self):
        args = examinee_parser_creator.parse_args()
        file = args.get('file')
        if not file:
            return {"error": "no file provided"}, 400

        name = args.get('name')
        if not name:
            return {"error": "no name provided"}, 400
        
        filename = file.filename
        file.save(os.path.join(current_app.config['UPLOAD_FOLDER'], filename))

        examinee = Examinee(name, filename)
        if args.get('id'):
            examinee.id = args.get('id')

        examinee.name = name 
        examinee.picture = filename

        db.session.add(examinee)

        ek = ExamKit(False, None)
        db.session.add(ek)
        db.session.flush()

        le = LogEntry(examinee.id)
        db.session.add(le)

        on_examinee_creation(examinee)

        db.session.commit()

        return utils.gen_success_message("new user created", examinee.to_dict())
    
    @api.expect(examinee_parser)
    def delete(self):
        args = examinee_parser.parse_args()
        id = args.get('id')
        if id is None:
            abort(HTTPStatus.BAD_REQUEST, "Missing ID Parameter")

        examinee = db.session.execute(db.select(Examinee).filter_by(id=id)).first()
        db.session.commit()

        if examinee is None:
            return {"error": "examinee does not exist"}, 400
        
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
    def post(self):
        args = logtime_args.parse_args()

        examinee_id = args.get('examinee_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        db.session.commit()

        if examinee is None:
            return {"error": "examinee does not exist"}, 404
        
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
    def post(self):
        args = timeout_args.parse_args()

        examinee_id = args.get('examinee_id')
        exam_kit_id = args.get('exam_kit_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        exam_kit = db.session.execute(db.select(ExamKit).filter_by(kit_id=exam_kit_id)).first()
        db.session.commit()

        if examinee is None:
            return {"error": "examinee does not exist"}, 404
        
        if exam_kit is None:
            return {"error": "exam kit does not exist"}, 404

        if examinee[0].id != exam_kit[0].examinee_id:
            return {"error": "exam kit examinee does not match"}, 400
        
        log_entry = db.session.query(LogEntry).filter_by(examinee=examinee_id).order_by(LogEntry.log_id.desc()).first()

        if log_entry:
            if log_entry.exam_time_in is None:
                return {"error": "examinee has not timed in"}, 400

            log_entry.exam_time_out = datetime.now()

        db.session.commit()

        return utils.gen_success_message("examinee has timed out", 200)
