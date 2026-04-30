from typing import Optional

def gen_success_message(msg: str, data: Optional[any]):
    dct = {
            "message": msg,
        }
    if data:
        dct['data'] = data

    return dct, 200

def gen_error(msg: str, status_code):
    return {'error': msg}, status_code
