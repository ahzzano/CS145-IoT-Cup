FROM python:3.13-slim

RUN apt-get update && apt-get install -y git libgl1 libglib2.0-0 && rm -rf /var/lib/apt/lists/*

WORKDIR /app/backend

RUN pip install uv

COPY pyproject.toml uv.lock ./

RUN uv sync --frozen

CMD ["uv", "run", "python", "app.py"]
