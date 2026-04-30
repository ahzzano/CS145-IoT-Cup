from flask_restx import Api
from .examinee import api as examinee_api
from .exam import api as exam_api
from .mosip import api as mosip_api

api = Api(
        title='Mock LERIS API',
        version='1.0'
    )

api.add_namespace(examinee_api)
api.add_namespace(exam_api)
api.add_namespace(mosip_api)

