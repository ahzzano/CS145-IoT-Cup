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

from apis import api

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
api.init_app(app)
migrate = Migrate(app, db)



def main():
    app.run(debug=True, port=8000)

if __name__ == "__main__":
    main()
