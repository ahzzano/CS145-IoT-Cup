from sqlalchemy import LargeBinary
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class Examinee(db.Model):
    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str]
    picture: Mapped[bytes] = mapped_column(LargeBinary)
