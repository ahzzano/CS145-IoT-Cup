from flask import Flask
from flask_restx import Api, Resource

app = Flask(__name__)
api = Api(app)

def main():
    print("Hello from backend!")
    app.run(debug=True)

if __name__ == "__main__":
    main()
