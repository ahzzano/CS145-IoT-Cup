"""dedupe picture and log rows

Revision ID: c7f9d4a0b6e2
Revises: a1b2c3d4e5f6
Create Date: 2026-05-06 00:00:00.000000

"""
from alembic import op


revision = 'c7f9d4a0b6e2'
down_revision = 'a1b2c3d4e5f6'
branch_labels = None
depends_on = None


def upgrade():
    op.execute("""
        WITH keep AS (
            SELECT examinee, MAX(log_id) AS keep_log_id
            FROM log_entry
            GROUP BY examinee
        ),
        rollup AS (
            SELECT
                examinee,
                MIN(reg_time) AS reg_time,
                MAX(exam_time_in) AS exam_time_in,
                MAX(exam_time_out) AS exam_time_out
            FROM log_entry
            GROUP BY examinee
        )
        UPDATE log_entry AS target
        SET
            reg_time = rollup.reg_time,
            exam_time_in = COALESCE(target.exam_time_in, rollup.exam_time_in),
            exam_time_out = COALESCE(target.exam_time_out, rollup.exam_time_out)
        FROM keep, rollup
        WHERE target.log_id = keep.keep_log_id
          AND rollup.examinee = keep.examinee
    """)

    op.execute("""
        DELETE FROM log_entry AS duplicate
        USING log_entry AS keeper
        WHERE duplicate.examinee = keeper.examinee
          AND duplicate.log_id < keeper.log_id
    """)

    op.execute("""
        DELETE FROM examinee_picture AS duplicate
        USING examinee_picture AS keeper
        WHERE duplicate.examinee_id = keeper.examinee_id
          AND duplicate.id < keeper.id
    """)

    with op.batch_alter_table('log_entry', schema=None) as batch_op:
        batch_op.create_unique_constraint('uq_log_entry_examinee', ['examinee'])

    with op.batch_alter_table('examinee_picture', schema=None) as batch_op:
        batch_op.create_unique_constraint('uq_examinee_picture_examinee_id', ['examinee_id'])


def downgrade():
    with op.batch_alter_table('examinee_picture', schema=None) as batch_op:
        batch_op.drop_constraint('uq_examinee_picture_examinee_id', type_='unique')

    with op.batch_alter_table('log_entry', schema=None) as batch_op:
        batch_op.drop_constraint('uq_log_entry_examinee', type_='unique')
