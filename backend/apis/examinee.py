import os
from dotenv import load_dotenv
from flask_restx.errors import HTTPStatus
from werkzeug.datastructures import FileStorage
load_dotenv()

from flask_restx import Namespace, Resource, abort, reqparse

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
            abort(HTTPStatus.BAD_REQUEST, "Missing ID Parameter")

        user = db.get_or_404(Examinee, id)
        return user.to_dict()
    
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
        file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))

        examinee = Examinee(name, filename)
        if args.get('id'):
            examinee.id = args.get('id')

        examinee.name = name 
        examinee.picture = filename

        db.session.add(examinee)
        db.session.commit()

        return {"status": "new user created"}, 200
    
    @api.expect(examinee_parser)
    def delete(self):
        args = examinee_parser.parse_args()
        id = args.get('id')
        if id is None:
            abort(HTTPStatus.BAD_REQUEST, "Missing ID Parameter")

        examinee = db.get_or_404(Examinee, id)
        db.session.delete(examinee)
        db.session.commit()

        return {"status": "deleted user"}, 200
