from datetime import datetime

from sqlalchemy import LargeBinary, ForeignKey, null
from sqlalchemy import func
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class LogEntry(db.Model):
    log_id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True, init=False)
    examinee: Mapped[int] = mapped_column(ForeignKey("examinee.id", ondelete='CASCADE'))
    reg_time: Mapped[datetime] = mapped_column(default=func.now())
    exam_time_in: Mapped[datetime] = mapped_column(nullable=True, default=None)
    exam_time_out: Mapped[datetime] = mapped_column(nullable=True, default=None)

"""
event types:
    examinee:
        examinee.created 
    examkit:
        examkit.linked
        examkit.unlinked
        examkit.submitted
"""
class EventLog(db.Model):
    event_type: Mapped[str]
    log_id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True, init=False)
    examinee: Mapped[int] = mapped_column(ForeignKey("examinee.id", ondelete='CASCADE'), nullable=True, default=None)
    exam_kit: Mapped[int] = mapped_column(ForeignKey('exam_kit.kit_id', ondelete='CASCADE'), nullable=True, default=None) 
    message: Mapped[str] = mapped_column(default="")
    timestamp: Mapped[datetime] = mapped_column(onupdate=func.now(), default=func.now())

