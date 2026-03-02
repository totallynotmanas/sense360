FROM python:3.11-slim

# Set environment variables
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV API_HOST=0.0.0.0
ENV API_PORT=5000

# Set work directory
WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy project files
COPY . .

# Create a non-root user and switch to it for better security
RUN useradd -m appuser && chown -R appuser /app
USER appuser

EXPOSE 5000

# Run entrypoint using gunicorn for production usage
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "--workers", "4", "app:app"]
