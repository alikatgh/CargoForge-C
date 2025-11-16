#!/bin/bash
# Quick start script for CargoForge-C Web Interface

echo "=========================================="
echo " CargoForge-C Web Interface Setup"
echo "=========================================="

# Check if in correct directory
if [ ! -f "../cargoforge" ]; then
    echo "Error: C binary not found. Building..."
    cd ..
    make clean && make
    cd web
fi

# Check Python
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required"
    exit 1
fi

# Install dependencies
echo "Installing Python dependencies..."
cd backend
pip3 install -r requirements.txt --quiet

# Start server
echo ""
echo "Starting web server..."
echo "Access at: http://localhost:5000"
echo ""
python3 app.py
