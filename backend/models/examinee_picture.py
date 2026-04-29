from sqlalchemy import LargeBinary, ForeignKey
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class ExamineePicture(db.Model):
    id: Mapped[int] = mapped_column(primary_key=True)
    pre_test: Mapped[bytes] = mapped_column(LargeBinary)
    post_test: Mapped[bytes] = mapped_column(LargeBinary)
    pre_conf: Mapped[float]
    post_conf: Mapped[float]
    examinee_id: Mapped[int] = mapped_column(ForeignKey("examinee.id"))
