from flask_restx import Api
from .examinee import api as examinee_api

api = Api(
        title='Mock LERIS API',
        version='1.0'
    )

api.add_namespace(examinee_api)


