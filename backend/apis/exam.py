from flask.json import jsonify
from auth import auth_required
from models import exam_kit
from models.base import db
from flask_restx import Namespace, Resource, fields, reqparse

from models.exam_kit import ExamKit
from models.examinee import *

import utils

api = Namespace('exam')

link_exam =  reqparse.RequestParser()
link_exam.add_argument('exam_id', location='form', type=int, required=True)

args_link_exam =  reqparse.RequestParser()
args_link_exam.add_argument('examinee', location='args', type=int, required=True)
args_link_exam.add_argument('exam_id', location='args', type=int, required=True)

@api.route('/link')
class ExamLinker(Resource):
    method_decorators = [auth_required]

    @api.expect(link_exam)
    def post(self, user):
        args = link_exam.parse_args()

        print(user)
        examinee_id = int(user['uin'])
        exam_id = args.get('exam_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        exam_kit = db.session.execute(db.select(ExamKit).filter_by(kit_id=exam_id)).first()
        db.session.commit()

        if examinee is None:
            return {"error": "examinee does not exist"}, 400
        
        if exam_kit is None:
            return {"error": "no more empty exam kits"}, 400

        ek = exam_kit[0]

        if ek.examinee_id != None:
            return {"error": "exam kit has already been linked"}, 400

        ek.examinee_id = examinee[0].id
        db.session.commit()

        return utils.gen_success_message("Exam Successfully Linked", ek.to_dict())
    
    @api.expect(args_link_exam)
    def get(self, user):
        args = args_link_exam.parse_args()

        examinee_id = args.get('examinee')
        exam_id = args.get('exam_id')

        examinee = db.session.execute(db.select(Examinee).filter_by(id=examinee_id)).first()
        exam_kit = db.session.execute(db.select(ExamKit).filter_by(kit_id=exam_id)).first()
        db.session.commit()

        if examinee is None:
            return {"error": "examinee does not exist"}, 404
        
        if exam_kit is None:
            return {"error": "exam kit does not exist"}, 404

        if exam_kit[0].examinee_id != examinee_id:
            return {'linked': False}, 200

        return {'linked': True}, 200

@api.route('/unlink')
class ExamUnlinker(Resource):
    @api.expect(link_exam)
    def post(self):
        args = link_exam.parse_args()
        exam_id = args.get('exam_id')

        exam_kit = db.session.execute(db.select(ExamKit).filter_by(kit_id=exam_id)).first()

        db.session.commit()

        if exam_kit is None:
            return {"error": "invalid exam kit"}, 400

        ek = exam_kit[0]

        ek.examinee_id = None
        db.session.commit()

        return ek.to_dict(), 200


@api.route('/submit')
class submit_exam(Resource):
    method_decorators = [auth_required]
    def post(self, user):
        "Submit an exam"
        examinee_id = int(user['uin'])

        exam_kit = db.session.execute(
                db.select(ExamKit).filter_by(examinee_id=examinee_id)
                ).first()

        db.session.commit()
        if exam_kit is None:
            return utils.gen_error("Examinee has no linked exam kit yet. Please link an exam kit first", 400)
        
        exam_kit[0].submitted = True
        db.session.commit()

        return utils.gen_success_message("Exam kit submitted", {})
