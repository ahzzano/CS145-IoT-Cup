from sqlalchemy.orm import Mapped, mapped_column
from base import db

class Examinee(db.Model):
    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str]
