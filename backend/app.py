import os
from dotenv import load_dotenv
from flask_restx.errors import HTTPException, HTTPStatus, ValidationError
from sqlalchemy import select
load_dotenv()

from flask import Flask, jsonify
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

db.init_app(app)
api = Api(app)
migrate = Migrate(app, db)

examinee_parser = reqparse.RequestParser()
examinee_parser.add_argument('id', type=int, help='id of the user', location='args')

@api.route('/examinee')
class GetExaminee(Resource):
    @api.expect(examinee_parser)
    def get(self):
        args = examinee_parser.parse_args()

        id = args.get('id')
        if id is None:
            abort(HTTPStatus.BAD_REQUEST, "Missing ID Parameter")

        user = db.session.execute(select(Examinee).filter_by(id=id)).first()
        if user is None:
            abort(HTTPStatus.NOT_FOUND, "User does not exist")

        return jsonify(user)

def main():
    app.run(debug=True)

if __name__ == "__main__":
    main()
