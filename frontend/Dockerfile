FROM node:22-alpine

WORKDIR /app/frontend

COPY package*.json ./

RUN npm install

CMD ["npm", "run", "dev", "--", "--host", "0.0.0.0"]
