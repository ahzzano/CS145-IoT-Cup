import flask_restx

from apis import exam
from models.base import db

from models.examinee import Examinee
from models.logs import EventLog

def on_examinee_creation(examinee: Examinee):
    db.session.flush()
    entry = EventLog(
            'examinee.created',
            message=f'Examinee {examinee.name} Created',
            examinee=examinee.id
        )

    db.session.add(entry)
