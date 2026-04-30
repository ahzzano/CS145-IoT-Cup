from datetime import datetime

from sqlalchemy import LargeBinary, ForeignKey, null
from sqlalchemy import func
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class LogEntry(db.Model):
    log_id: Mapped[int] = mapped_column(primary_key=True)
    examinee: Mapped[int] = mapped_column(ForeignKey("examinee.id"))
    reg_time: Mapped[datetime] = mapped_column(onupdate=func.now())
    exam_time_in: Mapped[datetime] = mapped_column(onupdate=func.now())
    exam_time_out: Mapped[datetime] = mapped_column(onupdate=func.now())

class EventLog(db.Model):
    log_id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True, init=False)
    examinee: Mapped[int] = mapped_column(ForeignKey("examinee.id"), nullable=True)
    exam_kit: Mapped[int] = mapped_column(ForeignKey('exam_kit.kit_id'))
    event_type: Mapped[str]
    timestamp: Mapped[datetime] = mapped_column(onupdate=func.now())

