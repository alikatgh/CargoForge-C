# Dockerfile for CargoForge-C Web Interface
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . /app/

# Build C binary
RUN make clean && make

# Install Python dependencies
RUN pip3 install --no-cache-dir -r web/backend/requirements.txt

# Expose port
EXPOSE 5000

# Run web server
CMD ["python3", "web/backend/app.py"]
