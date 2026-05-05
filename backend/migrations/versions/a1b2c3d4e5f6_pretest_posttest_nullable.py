"""allow pending post_test and post_conf on examinee_picture

Revision ID: a1b2c3d4e5f6
Revises: db1dc0988cf8
Create Date: 2026-05-05 18:00:00.000000

"""
from alembic import op
import sqlalchemy as sa


revision = 'a1b2c3d4e5f6'
down_revision = 'db1dc0988cf8'
branch_labels = None
depends_on = None


def upgrade():
    with op.batch_alter_table('examinee_picture', schema=None) as batch_op:
        batch_op.alter_column('post_test',
               existing_type=sa.LargeBinary(),
               nullable=True)
        batch_op.alter_column('post_conf',
               existing_type=sa.Float(),
               nullable=True)
        batch_op.alter_column('pre_conf',
               existing_type=sa.Float(),
               nullable=False,
               server_default=sa.text('0.0'))


def downgrade():
    with op.batch_alter_table('examinee_picture', schema=None) as batch_op:
        batch_op.alter_column('pre_conf',
               existing_type=sa.Float(),
               server_default=None)
        batch_op.alter_column('post_conf',
               existing_type=sa.Float(),
               nullable=False)
        batch_op.alter_column('post_test',
               existing_type=sa.LargeBinary(),
               nullable=False)
