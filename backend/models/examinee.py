from sqlalchemy import BigInteger, LargeBinary, ForeignKey
from sqlalchemy.orm import Mapped, mapped_column
from models.base import db

class Examinee(db.Model):
    id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True, init=False)
    name: Mapped[str]
    picture: Mapped[str]

    def to_dict(self):
        return {
            'id': self.id,
            'name': self.name,
            'picture': self.picture
        }
