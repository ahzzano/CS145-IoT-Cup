from sqlalchemy import BigInteger, ForeignKey, LargeBinary, null
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class ExamKit(db.Model):
    kit_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True, init=False)
    submitted: Mapped[bool]
    examinee_id: Mapped[int] = mapped_column(ForeignKey("examinee.id"), nullable=True)

    def to_dict(self):
        return { c.name: getattr(self, c.name) for c in self.__table__.columns }
    
