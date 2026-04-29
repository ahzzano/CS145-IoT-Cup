import os

from flask import current_app
from flask_restx.errors import HTTPStatus
from werkzeug.datastructures import FileStorage

from flask_restx import Namespace, Resource, abort, reqparse

from models.exam_kit import ExamKit
from models.examinee import *

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
            return {"error": "examinee does not exist"}, 400
        return user[0].to_dict()
    
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

        db.session.commit()

        return {"status": "new user created", 'examinee_id': examinee.id}, 200
    
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
        db.session.commit()

        return {"status": "deleted user"}, 200
