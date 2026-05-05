from sqlalchemy import BigInteger, LargeBinary, ForeignKey
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class ExamineePicture(db.Model):
    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True, init=False)
    pre_test: Mapped[bytes] = mapped_column(LargeBinary)
    examinee_id: Mapped[int] = mapped_column(ForeignKey("examinee.id"))
    post_test: Mapped[bytes] = mapped_column(LargeBinary, default=b'')
    pre_conf: Mapped[float] = mapped_column(default=0.0)
    post_conf: Mapped[float] = mapped_column(default=0.0)

    def to_dict(self):
        return {
            "id": self.id,
            "examinee_id": self.examinee_id,
            "has_pre_test": self.pre_test is not None,
            "has_post_test": bool(self.post_test),
            "pre_conf": self.pre_conf,
            "post_conf": self.post_conf,
        }
