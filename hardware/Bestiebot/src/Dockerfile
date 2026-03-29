FROM python:3.9-slim
WORKDIR /app
COPY . .
RUN pip install flask pymongo dnspython requests
EXPOSE 5001
CMD ["python", "app.py"]