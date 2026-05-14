from datetime import datetime, timezone, timedelta
from functools import wraps
from flask import request
import jwt
import utils
import os

SECRET_KEY = 'tuquequitaselpecadodelmundo'

bypass = os.getenv('bypassed')
def bypassed() -> bool:
    return bypass == 'true'

# jwt SHOULD HAVE the UIN value
def generate_jwt(user_data):
    token = jwt.encode({
            'uin': user_data['uin'],
            'name': user_data['name'],
            'exp': datetime.now(timezone.utc) + timedelta(hours=2)
        }, 
        SECRET_KEY, algorithm='HS256')
    return token

def auth_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        token = request.cookies.get('token') or request.headers.get('Authorization', '').replace('Bearer ', '')
        if bypassed():
            return f({'uin': 271670, 'name': 'Charlie Kirk'}, *args, **kwargs)

        if not token:
            return utils.gen_error("Token is missing", 401)
        
        try:
            data = jwt.decode(token, SECRET_KEY, algorithms='HS256')
            user = {
                    'uin': int(data['uin']),
                    'name': data['name']
                    }
        except jwt.ExpiredSignatureError:
            return utils.gen_error("Token has expired", 401)
        except:
            return utils.gen_error("Invalid token", 401)

        return f(user, *args, **kwargs)
    
    return decorated


