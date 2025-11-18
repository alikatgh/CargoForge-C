# CargoForge Production Deployment Guide

Complete guide for deploying CargoForge-C as a production SaaS application.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Database Configuration](#database-configuration)
4. [Stripe Payment Setup](#stripe-payment-setup)
5. [Docker Deployment](#docker-deployment)
6. [SSL/HTTPS Configuration](#ssl-https-configuration)
7. [Monitoring & Error Tracking](#monitoring--error-tracking)
8. [Backup & Recovery](#backup--recovery)
9. [Performance Optimization](#performance-optimization)
10. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Software

- **Docker** (20.10+) and Docker Compose (2.0+)
- **Git** for version control
- **Make** and **GCC** for building C binary
- **Domain name** with DNS access
- **SSL Certificate** (Let's Encrypt recommended)

### Required Accounts

- **Stripe Account** (for payments): https://dashboard.stripe.com/register
- **Sentry Account** (for error tracking): https://sentry.io/signup/
- **Email Service** (Gmail, SendGrid, or AWS SES)

### System Requirements

**Minimum (Development/Testing):**
- 2 CPU cores
- 4 GB RAM
- 20 GB storage

**Recommended (Production):**
- 4+ CPU cores
- 8+ GB RAM
- 100+ GB SSD storage
- Backup storage

---

## Environment Setup

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/CargoForge-C.git
cd CargoForge-C
```

### 2. Build C Binary

```bash
make clean
make
make test

# Verify binary works
./cargoforge examples/example_ship.txt examples/example_cargo.txt --json
```

### 3. Configure Environment Variables

Copy the example environment file and edit it:

```bash
cp .env.example .env
nano .env
```

**Required Environment Variables:**

```bash
# Security (CRITICAL - Generate strong keys!)
SECRET_KEY=your-secret-key-here-change-this-in-production

# Database (Production)
DATABASE_URL=postgresql://cargoforge:STRONG_PASSWORD@db:5432/cargoforge

# Flask
FLASK_APP=web/backend/app_prod.py
FLASK_ENV=production

# Redis (for rate limiting)
REDIS_URL=redis://redis:6379/0

# Domain
DOMAIN=yourdomain.com
ALLOWED_ORIGINS=https://yourdomain.com,https://www.yourdomain.com
```

**Generate Strong Secret Key:**

```bash
python3 -c "import secrets; print(secrets.token_hex(32))"
```

---

## Database Configuration

### PostgreSQL Setup

The docker-compose.yml includes PostgreSQL, but for production you may want a managed service:

**Managed Database Options:**
- AWS RDS
- Google Cloud SQL
- DigitalOcean Managed Databases
- Heroku Postgres

**Update DATABASE_URL:**

```bash
# Format: postgresql://username:password@host:port/database
DATABASE_URL=postgresql://user:pass@db-host.example.com:5432/cargoforge
```

### Database Initialization

The application automatically creates tables on first run:

```python
# In app_prod.py
with app.app_context():
    db.create_all()
```

### Database Migrations

For schema changes, use Flask-Migrate (optional):

```bash
pip install flask-migrate
flask db init
flask db migrate -m "Initial migration"
flask db upgrade
```

### Backup Strategy

**Daily Backups:**

```bash
# Create backup script: /opt/cargoforge/backup.sh
#!/bin/bash
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR=/backups/cargoforge
mkdir -p $BACKUP_DIR

# PostgreSQL backup
docker-compose exec -T db pg_dump -U cargoforge cargoforge > \
    $BACKUP_DIR/cargoforge_$DATE.sql

# Compress
gzip $BACKUP_DIR/cargoforge_$DATE.sql

# Delete backups older than 30 days
find $BACKUP_DIR -name "*.sql.gz" -mtime +30 -delete
```

**Cron Job:**

```bash
0 2 * * * /opt/cargoforge/backup.sh >> /var/log/cargoforge_backup.log 2>&1
```

---

## Stripe Payment Setup

### 1. Create Stripe Account

Sign up at https://dashboard.stripe.com/register

### 2. Get API Keys

**Development (Test Mode):**
- Publishable Key: `pk_test_...`
- Secret Key: `sk_test_...`

**Production (Live Mode):**
- Publishable Key: `pk_live_...`
- Secret Key: `sk_live_...`

### 3. Create Products and Prices

In Stripe Dashboard:

1. Go to **Products** → **Add Product**

2. Create **Pro Monthly** product:
   - Name: "CargoForge Pro (Monthly)"
   - Price: $29/month
   - Billing period: Monthly
   - Copy the Price ID (e.g., `price_1234567890`)

3. Create **Pro Annual** product:
   - Name: "CargoForge Pro (Annual)"
   - Price: $290/year
   - Billing period: Yearly
   - Copy the Price ID

### 4. Configure Environment Variables

Add to `.env`:

```bash
# Stripe Keys
STRIPE_PUBLIC_KEY=pk_live_your_key_here
STRIPE_SECRET_KEY=sk_live_your_key_here
STRIPE_WEBHOOK_SECRET=whsec_your_webhook_secret

# Stripe Price IDs
STRIPE_PRICE_PRO_MONTHLY=price_1234567890
STRIPE_PRICE_PRO_ANNUAL=price_0987654321
```

### 5. Configure Webhook

1. Go to **Developers** → **Webhooks** → **Add endpoint**

2. **Endpoint URL:** `https://yourdomain.com/api/billing/webhook`

3. **Events to send:**
   - `checkout.session.completed`
   - `invoice.payment_succeeded`
   - `invoice.payment_failed`
   - `customer.subscription.updated`
   - `customer.subscription.deleted`

4. Copy **Signing secret** to `STRIPE_WEBHOOK_SECRET`

### 6. Test Webhook Locally

Use Stripe CLI:

```bash
# Install Stripe CLI
brew install stripe/stripe-cli/stripe

# Login
stripe login

# Forward webhooks to local server
stripe listen --forward-to localhost:5000/api/billing/webhook

# Test webhook
stripe trigger checkout.session.completed
```

---

## Docker Deployment

### 1. Review docker-compose.yml

```yaml
version: '3.8'

services:
  web:
    build: .
    ports:
      - "5000:5000"
    environment:
      - FLASK_ENV=production
      - DATABASE_URL=${DATABASE_URL}
      - REDIS_URL=${REDIS_URL}
      - STRIPE_SECRET_KEY=${STRIPE_SECRET_KEY}
    depends_on:
      - db
      - redis
    restart: unless-stopped

  db:
    image: postgres:15-alpine
    environment:
      - POSTGRES_DB=cargoforge
      - POSTGRES_USER=cargoforge
      - POSTGRES_PASSWORD=${DB_PASSWORD}
    volumes:
      - postgres_data:/var/lib/postgresql/data
    restart: unless-stopped

  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
    restart: unless-stopped

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./ssl:/etc/nginx/ssl:ro
    depends_on:
      - web
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:
```

### 2. Create Dockerfile

Already included in repository:

```dockerfile
FROM python:3.11-slim

WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    postgresql-client \
    && rm -rf /var/lib/apt/lists/*

# Copy project
COPY . /app

# Build C binary
RUN make clean && make

# Install Python dependencies
RUN pip install --no-cache-dir -r web/backend/requirements-prod.txt

# Expose port
EXPOSE 5000

# Run application
CMD ["gunicorn", "-w", "4", "-b", "0.0.0.0:5000", "web.backend.app_prod:app"]
```

### 3. Deploy with Docker Compose

```bash
# Build and start all services
docker-compose build
docker-compose up -d

# View logs
docker-compose logs -f

# Check status
docker-compose ps

# Stop services
docker-compose down
```

### 4. Automated Deployment Script

Use the included `deploy.sh`:

```bash
chmod +x scripts/deploy.sh
./scripts/deploy.sh
```

The script will:
1. Build C binary
2. Run tests
3. Build Docker images
4. Run database migrations
5. Start services
6. Perform health check

---

## SSL/HTTPS Configuration

### 1. Obtain SSL Certificate

**Using Let's Encrypt (Recommended):**

```bash
# Install certbot
sudo apt-get update
sudo apt-get install certbot

# Get certificate
sudo certbot certonly --standalone -d yourdomain.com -d www.yourdomain.com

# Certificates will be saved to:
# /etc/letsencrypt/live/yourdomain.com/fullchain.pem
# /etc/letsencrypt/live/yourdomain.com/privkey.pem
```

### 2. Configure Nginx

Create `nginx.conf`:

```nginx
events {
    worker_connections 1024;
}

http {
    upstream cargoforge_backend {
        server web:5000;
    }

    # Redirect HTTP to HTTPS
    server {
        listen 80;
        server_name yourdomain.com www.yourdomain.com;
        return 301 https://$server_name$request_uri;
    }

    # HTTPS server
    server {
        listen 443 ssl http2;
        server_name yourdomain.com www.yourdomain.com;

        ssl_certificate /etc/nginx/ssl/fullchain.pem;
        ssl_certificate_key /etc/nginx/ssl/privkey.pem;

        # SSL Configuration
        ssl_protocols TLSv1.2 TLSv1.3;
        ssl_ciphers HIGH:!aNULL:!MD5;
        ssl_prefer_server_ciphers on;

        # Security headers
        add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
        add_header X-Frame-Options "SAMEORIGIN" always;
        add_header X-Content-Type-Options "nosniff" always;

        # Proxy settings
        location / {
            proxy_pass http://cargoforge_backend;
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
        }

        # Static files caching
        location ~* \.(jpg|jpeg|png|gif|ico|css|js)$ {
            proxy_pass http://cargoforge_backend;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }
    }
}
```

### 3. Copy SSL Certificates

```bash
mkdir -p ssl
sudo cp /etc/letsencrypt/live/yourdomain.com/fullchain.pem ssl/
sudo cp /etc/letsencrypt/live/yourdomain.com/privkey.pem ssl/
sudo chown -R $USER:$USER ssl/
```

### 4. Auto-Renewal

```bash
# Add cron job for certificate renewal
0 0 1 * * sudo certbot renew --quiet && docker-compose restart nginx
```

---

## Monitoring & Error Tracking

### 1. Sentry Setup

**Sign up:** https://sentry.io/signup/

**Get DSN:** Project Settings → Client Keys (DSN)

**Add to .env:**

```bash
SENTRY_DSN=https://abc123@o123456.ingest.sentry.io/7654321
```

**The app already includes Sentry:**

```python
import sentry_sdk
from sentry_sdk.integrations.flask import FlaskIntegration

sentry_sdk.init(
    dsn=os.getenv('SENTRY_DSN'),
    integrations=[FlaskIntegration()],
    traces_sample_rate=0.1,
    environment='production'
)
```

### 2. Application Logging

Configure logging in production:

```python
import logging
from logging.handlers import RotatingFileHandler

if not app.debug:
    file_handler = RotatingFileHandler(
        'logs/cargoforge.log',
        maxBytes=10240000,
        backupCount=10
    )
    file_handler.setFormatter(logging.Formatter(
        '%(asctime)s %(levelname)s: %(message)s [in %(pathname)s:%(lineno)d]'
    ))
    file_handler.setLevel(logging.INFO)
    app.logger.addHandler(file_handler)
    app.logger.setLevel(logging.INFO)
    app.logger.info('CargoForge startup')
```

### 3. Health Check Endpoint

Add to app_prod.py:

```python
@app.route('/health')
def health_check():
    """Health check endpoint for monitoring"""
    try:
        # Check database
        db.session.execute('SELECT 1')

        # Check C binary
        binary_ok = os.path.exists(CARGOFORGE_BIN)

        return jsonify({
            'status': 'healthy',
            'database': 'connected',
            'binary': 'available' if binary_ok else 'missing'
        }), 200
    except Exception as e:
        return jsonify({
            'status': 'unhealthy',
            'error': str(e)
        }), 500
```

### 4. Uptime Monitoring

Use external services:
- **UptimeRobot** (free): https://uptimerobot.com
- **Pingdom**: https://www.pingdom.com
- **StatusCake**: https://www.statuscake.com

Monitor: `https://yourdomain.com/health`

---

## Backup & Recovery

### Database Backup

See [Database Configuration](#database-configuration) section.

### Disaster Recovery Plan

1. **Regular backups** (automated daily)
2. **Off-site storage** (S3, Google Cloud Storage)
3. **Test restores** monthly
4. **Documentation** of recovery procedures

**Recovery Procedure:**

```bash
# Stop application
docker-compose down

# Restore database
gunzip < /backups/cargoforge_YYYYMMDD.sql.gz | \
    docker-compose exec -T db psql -U cargoforge cargoforge

# Restart application
docker-compose up -d
```

---

## Performance Optimization

### 1. Gunicorn Workers

Calculate workers: `(2 × CPU_CORES) + 1`

Update `docker-compose.yml`:

```yaml
command: gunicorn -w 9 -b 0.0.0.0:5000 "web.backend.app_prod:app"
```

### 2. PostgreSQL Tuning

For 8GB RAM server, add to `docker-compose.yml`:

```yaml
db:
  command:
    - "postgres"
    - "-c"
    - "shared_buffers=2GB"
    - "-c"
    - "effective_cache_size=6GB"
    - "-c"
    - "maintenance_work_mem=512MB"
    - "-c"
    - "max_connections=100"
```

### 3. Redis Configuration

Add to `docker-compose.yml`:

```yaml
redis:
  command: redis-server --maxmemory 256mb --maxmemory-policy allkeys-lru
```

### 4. CDN for Static Assets

Use CloudFlare, AWS CloudFront, or similar for:
- Frontend assets (JS, CSS)
- Images and media files
- PDF exports (cache)

---

## Troubleshooting

### Common Issues

**1. Database Connection Failed**

```bash
# Check if database is running
docker-compose ps

# Check database logs
docker-compose logs db

# Verify DATABASE_URL in .env
echo $DATABASE_URL
```

**2. Stripe Webhook Failures**

```bash
# Check webhook secret
echo $STRIPE_WEBHOOK_SECRET

# Test webhook locally
stripe listen --forward-to localhost:5000/api/billing/webhook

# Check application logs
docker-compose logs web | grep webhook
```

**3. C Binary Not Found**

```bash
# Rebuild binary
make clean && make

# Verify binary
./cargoforge --version

# Check in Docker
docker-compose exec web ls -la /app/cargoforge
```

**4. PDF Generation Errors**

```bash
# Check reportlab installation
docker-compose exec web pip show reportlab

# Test PDF generation
docker-compose exec web python -c "from pdf_export import generate_pdf_report; print('OK')"
```

**5. Rate Limiting Issues**

```bash
# Check Redis connection
docker-compose exec redis redis-cli ping

# Clear rate limit for user (development only)
docker-compose exec redis redis-cli FLUSHALL
```

### Performance Issues

**High Memory Usage:**

```bash
# Check container stats
docker stats

# Reduce Gunicorn workers
# Edit docker-compose.yml: -w 4 instead of -w 9
```

**Slow Database Queries:**

```bash
# Enable query logging
# In docker-compose.yml:
db:
  command: postgres -c log_statement=all

# Analyze slow queries
docker-compose exec db psql -U cargoforge -c "SELECT * FROM pg_stat_statements ORDER BY mean_time DESC LIMIT 10;"
```

### Logs

```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f web
docker-compose logs -f db

# Last 100 lines
docker-compose logs --tail=100 web

# Filter by time
docker-compose logs --since 30m web
```

---

## Production Checklist

Before going live:

- [ ] Strong SECRET_KEY generated
- [ ] Production database configured
- [ ] Stripe live keys configured
- [ ] SSL certificate installed
- [ ] Nginx HTTPS configured
- [ ] Sentry error tracking active
- [ ] Automated backups configured
- [ ] Health checks monitoring active
- [ ] DNS records configured
- [ ] Rate limiting tested
- [ ] Payment flow tested (test mode)
- [ ] Email notifications configured
- [ ] Terms of Service accessible
- [ ] Privacy Policy accessible
- [ ] Load testing completed
- [ ] Disaster recovery plan documented
- [ ] Team trained on deployment process

---

## Support & Maintenance

### Regular Tasks

**Weekly:**
- Review error logs in Sentry
- Check uptime monitoring
- Review user feedback

**Monthly:**
- Test backup restoration
- Review and optimize database
- Update dependencies
- Security audit

**Quarterly:**
- Review and update pricing
- Analyze user growth metrics
- Infrastructure capacity planning

### Updates

```bash
# Pull latest changes
git pull origin main

# Rebuild and redeploy
docker-compose build
docker-compose up -d

# Run database migrations if needed
docker-compose exec web flask db upgrade
```

---

## Additional Resources

- **Flask Documentation:** https://flask.palletsprojects.com/
- **Docker Documentation:** https://docs.docker.com/
- **Stripe API:** https://stripe.com/docs/api
- **PostgreSQL Tuning:** https://pgtune.leopard.in.ua/
- **Let's Encrypt:** https://letsencrypt.org/getting-started/
- **Sentry Documentation:** https://docs.sentry.io/

---

## Getting Help

- **GitHub Issues:** https://github.com/yourusername/CargoForge-C/issues
- **Email:** support@cargoforge.com
- **Documentation:** https://docs.cargoforge.com

---

**Last Updated:** 2025-11-16
**Version:** 1.0.0
