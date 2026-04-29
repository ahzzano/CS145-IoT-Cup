import os
from dotenv import load_dotenv
from flask_restx.errors import HTTPException, HTTPStatus, ValidationError
from sqlalchemy import select
import werkzeug
from werkzeug.datastructures import FileStorage
load_dotenv()

from flask import Flask, jsonify, request
from flask_restx import Api, Resource, abort, reqparse
from flask_migrate import Migrate

from models.base import db
from models.examinee import *
from models.exam_kit import *
from models.examinee_picture import *
from models.logs import *

app = Flask(__name__)

# DATABASE CREDENTIALS
USER = os.getenv("user")
PASSWORD = os.getenv("password")
HOST = os.getenv("host")
PORT = os.getenv("port")
DBNAME = os.getenv("dbname")

DATABASE_URL = f'postgresql+psycopg://{USER}:{PASSWORD}@{HOST}:{PORT}/{DBNAME}?sslmode=require'

app.config['SQLALCHEMY_DATABASE_URI'] = DATABASE_URL
app.config['UPLOAD_FOLDER'] = 'serve/'

db.init_app(app)
api = Api(app)
migrate = Migrate(app, db)

examinee_parser = reqparse.RequestParser()
examinee_parser.add_argument('id', type=int, help='id of the user', location='args')

examinee_parser_creator = reqparse.RequestParser()
examinee_parser_creator.add_argument('file', location='files', type=FileStorage, required=True)
examinee_parser_creator.add_argument('name', location='form', type=str, required=True)
examinee_parser_creator.add_argument('id', type=int,location='form')

@api.route('/examinee')
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


def main():
    app.run(debug=True, port=8000)

if __name__ == "__main__":
    main()
