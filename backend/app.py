import os
from dotenv import load_dotenv
load_dotenv()

from flask import Flask
from flask_restx import Api, Resource
from flask_migrate import Migrate

from models.base import db

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

import models.examinee
import models.exam_kit
import models.examinee_picture
import models.logs

def init_database():
    with app.app_context():
        db.create_all()
        migrate.init_app(app, db)

def main():
    init_database()
    app.run(debug=True)

if __name__ == "__main__":
    main()
