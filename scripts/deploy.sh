#!/bin/bash
# Production Deployment Script for CargoForge-C

set -e  # Exit on error

echo "========================================"
echo "  CargoForge Production Deployment"
echo "========================================"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check environment
if [ ! -f ".env" ]; then
    echo -e "${YELLOW}Warning: .env file not found. Creating from template...${NC}"
    cp .env.example .env
    echo -e "${YELLOW}Please edit .env with your production values before continuing!${NC}"
    exit 1
fi

# Build C binary
echo -e "${GREEN}Building C binary...${NC}"
make clean && make
chmod +x cargoforge

# Run tests
echo -e "${GREEN}Running tests...${NC}"
make test
if [ $? -ne 0 ]; then
    echo "Tests failed! Fix before deploying."
    exit 1
fi

# Build Docker images
echo -e "${GREEN}Building Docker images...${NC}"
docker-compose build

# Database migration
echo -e "${GREEN}Running database migrations...${NC}"
docker-compose run --rm web python -c "from web.backend.app_prod import db; db.create_all()"

# Start services
echo -e "${GREEN}Starting services...${NC}"
docker-compose up -d

# Wait for services to be healthy
echo -e "${GREEN}Waiting for services to start...${NC}"
sleep 10

# Health check
echo -e "${GREEN}Performing health check...${NC}"
curl -f http://localhost:5000/api/challenges || {
    echo "Health check failed!"
    docker-compose logs
    exit 1
}

echo ""
echo -e "${GREEN}========================================"
echo "  Deployment Successful! ✓"
echo "========================================${NC}"
echo ""
echo "Services running:"
echo "  - Web App: http://localhost:5000"
echo "  - Database: localhost:5432"
echo "  - Redis: localhost:6379"
echo ""
echo "View logs: docker-compose logs -f"
echo "Stop services: docker-compose down"
echo ""
