from sqlalchemy import ForeignKey, LargeBinary
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class ExamKit(db.Model):
    kit_id: Mapped[int] = mapped_column(primary_key=True)
    submitted: Mapped[bool]
    examinee_id: Mapped[int] = mapped_column(ForeignKey("examinee.id"))
    
