import os

from flask import current_app
from flask_restx.errors import HTTPStatus
from werkzeug.datastructures import FileStorage

from flask_restx import Namespace, Resource, abort, reqparse

from models.exam_kit import ExamKit
from models.examinee import *

api = Namespace("mosip", description='all MOSIP related requests')

@api.route('/auth')
class Auth(Resource):
    def post(self):
        return {}, 200
