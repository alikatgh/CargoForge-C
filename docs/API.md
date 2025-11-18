# CargoForge API Documentation

REST API documentation for CargoForge maritime cargo optimization platform.

**Base URL:** `https://api.cargoforge.com` (or your domain)
**Version:** 1.0.0
**Authentication:** API Key or Session Cookie

---

## Table of Contents

1. [Authentication](#authentication)
2. [Rate Limiting](#rate-limiting)
3. [Error Handling](#error-handling)
4. [Endpoints](#endpoints)
   - [Authentication](#authentication-endpoints)
   - [Optimization](#optimization-endpoints)
   - [Scenarios](#scenario-endpoints)
   - [Export](#export-endpoints)
   - [Billing](#billing-endpoints)
5. [WebhookS](#webhooks)
6. [Code Examples](#code-examples)

---

## Authentication

CargoForge API supports two authentication methods:

### 1. API Key (Recommended for integrations)

Include your API key in the request header:

```
X-API-Key: your_api_key_here
```

**Get your API key:**
1. Sign up at https://cargoforge.com
2. Navigate to Settings → API Keys
3. Generate new API key

### 2. Session Cookie (Web applications)

Use session-based authentication after logging in via `/api/auth/login`.

---

## Rate Limiting

Rate limits vary by subscription tier:

| Tier       | Daily Limit | Per Minute |
|------------|-------------|------------|
| Free       | 10          | 5          |
| Pro        | 1,000       | 30         |
| Enterprise | 10,000      | 100        |

**Rate limit headers:**

```
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 999
X-RateLimit-Reset: 1640995200
```

**Rate limit exceeded response:**

```json
{
  "error": "Rate limit exceeded",
  "retry_after": 60
}
```

---

## Error Handling

All errors return JSON with an `error` field:

```json
{
  "error": "Description of what went wrong"
}
```

### HTTP Status Codes

| Code | Meaning                 |
|------|-------------------------|
| 200  | Success                 |
| 201  | Created                 |
| 400  | Bad Request             |
| 401  | Unauthorized            |
| 403  | Forbidden               |
| 404  | Not Found               |
| 429  | Too Many Requests       |
| 500  | Internal Server Error   |

---

## Endpoints

### Authentication Endpoints

#### Register User

```
POST /api/auth/register
```

**Request body:**

```json
{
  "email": "user@example.com",
  "password": "secure_password",
  "username": "optional_username"
}
```

**Response:**

```json
{
  "success": true,
  "user": {
    "id": 1,
    "email": "user@example.com",
    "username": "user",
    "subscription_tier": "free",
    "api_key": "sk_live_abc123..."
  }
}
```

**Rate limit:** 5 per hour

---

#### Login

```
POST /api/auth/login
```

**Request body:**

```json
{
  "email": "user@example.com",
  "password": "secure_password"
}
```

**Response:**

```json
{
  "success": true,
  "user": {
    "id": 1,
    "email": "user@example.com",
    "subscription_tier": "pro"
  }
}
```

**Rate limit:** 10 per hour

---

#### Logout

```
POST /api/auth/logout
```

**Authentication:** Required

**Response:**

```json
{
  "success": true
}
```

---

#### Get Current User

```
GET /api/auth/me
```

**Authentication:** Required

**Response:**

```json
{
  "user": {
    "id": 1,
    "email": "user@example.com",
    "username": "user",
    "subscription_tier": "pro",
    "api_key": "sk_live_abc123...",
    "daily_limit": 1000
  }
}
```

---

### Optimization Endpoints

#### Run Optimization

```
POST /api/optimize
```

**Authentication:** Required
**Rate limit:** 30 per minute

**Request body:**

```json
{
  "ship": {
    "length": 150.0,
    "width": 25.0,
    "depth": 12.0,
    "max_draft": 10.0,
    "displacement": 50000.0
  },
  "cargo": [
    {
      "id": "Container1",
      "weight": 25000.0,
      "dimensions": [12.0, 2.4, 2.6],
      "type": "standard"
    },
    {
      "id": "Container2",
      "weight": 30000.0,
      "dimensions": [12.0, 2.4, 2.6],
      "type": "hazardous"
    }
  ]
}
```

**Cargo types:** `standard`, `hazardous`, `reefer`, `fragile`, `heavy`, `oversized`

**Response:**

```json
{
  "ship": {
    "length": 150.0,
    "width": 25.0,
    "depth": 12.0
  },
  "cargo": [
    {
      "id": "Container1",
      "weight": 25000.0,
      "dimensions": [12.0, 2.4, 2.6],
      "type": "standard",
      "placed": true,
      "x": 10.5,
      "y": 5.2,
      "z": 0.0
    },
    {
      "id": "Container2",
      "weight": 30000.0,
      "dimensions": [12.0, 2.4, 2.6],
      "type": "hazardous",
      "placed": true,
      "x": 25.0,
      "y": 8.0,
      "z": 0.0
    }
  ],
  "analysis": {
    "gm": 2.45,
    "kg": 5.8,
    "longitudinal_balance": 2.3,
    "lateral_balance": 0.5,
    "placed_count": 2,
    "total_count": 2,
    "cargo_utilization": 85.5,
    "weight_utilization": 90.2,
    "stability_status": "optimal"
  }
}
```

**Stability status values:**
- `optimal`: GM between 0.5m and 3.0m
- `unstable`: GM < 0 (critical)
- `low_stability`: GM < 0.3m (warning)
- `over_stiff`: GM > 3.0m (warning)

---

#### Get Training Challenges

```
GET /api/challenges
```

**Authentication:** Not required

**Response:**

```json
{
  "challenges": [
    {
      "id": 1,
      "title": "Getting Started",
      "difficulty": "beginner",
      "description": "Learn the basics...",
      "ship": {...},
      "cargo": [...],
      "goals": {
        "cg_range": [40, 60],
        "min_gm": 0.5,
        "all_placed": true
      }
    }
  ]
}
```

---

### Scenario Endpoints

#### List Scenarios

```
GET /api/scenarios
```

**Authentication:** Required

**Response:**

```json
{
  "scenarios": [
    {
      "id": 1,
      "name": "Container Ship Load Plan",
      "description": "Standard 20ft containers",
      "created_at": "2025-11-16T10:30:00Z",
      "updated_at": "2025-11-16T10:30:00Z",
      "is_public": false,
      "share_token": "abc123..."
    }
  ]
}
```

---

#### Create Scenario

```
POST /api/scenarios
```

**Authentication:** Required
**Rate limit:** 20 per hour

**Request body:**

```json
{
  "name": "My Cargo Plan",
  "description": "Description here",
  "ship": {...},
  "cargo": [...],
  "results": {...},
  "is_public": false
}
```

**Response:**

```json
{
  "scenario": {
    "id": 1,
    "name": "My Cargo Plan",
    "share_token": "abc123...",
    "created_at": "2025-11-16T10:30:00Z"
  }
}
```

---

#### Get Scenario

```
GET /api/scenarios/:id
```

**Authentication:** Required (or public scenario)

**Response:**

```json
{
  "scenario": {
    "id": 1,
    "name": "My Cargo Plan",
    "description": "Description",
    "ship_config": {...},
    "cargo_manifest": [...],
    "results": {...},
    "created_at": "2025-11-16T10:30:00Z"
  }
}
```

---

#### Update Scenario

```
PUT /api/scenarios/:id
```

**Authentication:** Required (owner only)

**Request body:** Same as create

**Response:** Updated scenario object

---

#### Delete Scenario

```
DELETE /api/scenarios/:id
```

**Authentication:** Required (owner only)

**Response:**

```json
{
  "success": true
}
```

---

#### Share Scenario

```
GET /api/scenarios/shared/:token
```

**Authentication:** Not required

Access public scenarios via share token.

---

### Export Endpoints

#### Export PDF

```
POST /api/export/pdf
```

**Authentication:** Required
**Rate limit:** 10 per hour

**Request body:**

```json
{
  "ship": {...},
  "cargo": [...],
  "scenario_name": "My Load Plan"
}
```

**Response:** PDF file download

**Headers:**
```
Content-Type: application/pdf
Content-Disposition: attachment; filename="cargo_loading_plan_My_Load_Plan.pdf"
```

---

#### Export Scenario PDF

```
GET /api/export/pdf/:scenario_id
```

**Authentication:** Required
**Rate limit:** 10 per hour

Export saved scenario as PDF.

**Response:** PDF file download

---

### Billing Endpoints

#### Get Pricing

```
GET /api/billing/pricing
```

**Authentication:** Not required

**Response:**

```json
{
  "free": {
    "name": "Free",
    "price": 0,
    "currency": "USD",
    "daily_limit": 10,
    "features": [...]
  },
  "pro_monthly": {
    "name": "Pro (Monthly)",
    "price": 29,
    "currency": "USD",
    "interval": "month",
    "daily_limit": 1000,
    "stripe_price_id": "price_...",
    "features": [...]
  }
}
```

---

#### Create Checkout Session

```
POST /api/billing/checkout
```

**Authentication:** Required
**Rate limit:** 5 per hour

**Request body:**

```json
{
  "tier": "pro_monthly"
}
```

**Response:**

```json
{
  "checkout_url": "https://checkout.stripe.com/...",
  "session_id": "cs_test_..."
}
```

Redirect user to `checkout_url` to complete payment.

---

#### Access Billing Portal

```
POST /api/billing/portal
```

**Authentication:** Required
**Rate limit:** 10 per hour

**Response:**

```json
{
  "portal_url": "https://billing.stripe.com/..."
}
```

Users can manage subscription, update payment method, view invoices.

---

#### Cancel Subscription

```
POST /api/billing/cancel
```

**Authentication:** Required
**Rate limit:** 5 per hour

**Response:**

```json
{
  "success": true,
  "message": "Subscription will be cancelled at period end",
  "period_end": 1672531200
}
```

---

#### Reactivate Subscription

```
POST /api/billing/reactivate
```

**Authentication:** Required
**Rate limit:** 5 per hour

**Response:**

```json
{
  "success": true,
  "message": "Subscription reactivated successfully"
}
```

---

## Webhooks

### Stripe Webhook

```
POST /api/billing/webhook
```

**Authentication:** Stripe signature verification

**Events handled:**
- `checkout.session.completed` - User subscribed
- `invoice.payment_succeeded` - Payment received
- `invoice.payment_failed` - Payment failed
- `customer.subscription.updated` - Subscription changed
- `customer.subscription.deleted` - Subscription cancelled

Configure in Stripe Dashboard: Developers → Webhooks

---

## Code Examples

### Python

```python
import requests

API_URL = "https://api.cargoforge.com"
API_KEY = "your_api_key_here"

headers = {
    "X-API-Key": API_KEY,
    "Content-Type": "application/json"
}

# Run optimization
response = requests.post(
    f"{API_URL}/api/optimize",
    headers=headers,
    json={
        "ship": {
            "length": 150,
            "width": 25,
            "depth": 12,
            "max_draft": 10,
            "displacement": 50000
        },
        "cargo": [
            {
                "id": "Container1",
                "weight": 25000,
                "dimensions": [12, 2.4, 2.6],
                "type": "standard"
            }
        ]
    }
)

result = response.json()
print(f"GM: {result['analysis']['gm']}m")
print(f"Placed: {result['analysis']['placed_count']}/{result['analysis']['total_count']}")
```

### JavaScript/Node.js

```javascript
const axios = require('axios');

const API_URL = 'https://api.cargoforge.com';
const API_KEY = 'your_api_key_here';

async function optimizeCargo() {
  try {
    const response = await axios.post(
      `${API_URL}/api/optimize`,
      {
        ship: {
          length: 150,
          width: 25,
          depth: 12,
          max_draft: 10,
          displacement: 50000
        },
        cargo: [
          {
            id: 'Container1',
            weight: 25000,
            dimensions: [12, 2.4, 2.6],
            type: 'standard'
          }
        ]
      },
      {
        headers: {
          'X-API-Key': API_KEY,
          'Content-Type': 'application/json'
        }
      }
    );

    const { analysis } = response.data;
    console.log(`GM: ${analysis.gm}m`);
    console.log(`Placed: ${analysis.placed_count}/${analysis.total_count}`);
  } catch (error) {
    console.error('Error:', error.response.data.error);
  }
}

optimizeCargo();
```

### cURL

```bash
curl -X POST https://api.cargoforge.com/api/optimize \
  -H "X-API-Key: your_api_key_here" \
  -H "Content-Type: application/json" \
  -d '{
    "ship": {
      "length": 150,
      "width": 25,
      "depth": 12,
      "max_draft": 10,
      "displacement": 50000
    },
    "cargo": [
      {
        "id": "Container1",
        "weight": 25000,
        "dimensions": [12, 2.4, 2.6],
        "type": "standard"
      }
    ]
  }'
```

### PHP

```php
<?php
$api_url = 'https://api.cargoforge.com';
$api_key = 'your_api_key_here';

$data = [
    'ship' => [
        'length' => 150,
        'width' => 25,
        'depth' => 12,
        'max_draft' => 10,
        'displacement' => 50000
    ],
    'cargo' => [
        [
            'id' => 'Container1',
            'weight' => 25000,
            'dimensions' => [12, 2.4, 2.6],
            'type' => 'standard'
        ]
    ]
];

$ch = curl_init($api_url . '/api/optimize');
curl_setopt($ch, CURLOPT_POST, 1);
curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($data));
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    'X-API-Key: ' . $api_key,
    'Content-Type: application/json'
]);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$response = curl_exec($ch);
$result = json_decode($response, true);

echo "GM: " . $result['analysis']['gm'] . "m\n";
echo "Placed: " . $result['analysis']['placed_count'] . "/" . $result['analysis']['total_count'] . "\n";

curl_close($ch);
?>
```

---

## Best Practices

1. **Store API keys securely** - Never commit to version control
2. **Handle rate limits** - Implement exponential backoff
3. **Validate input** - Check ship/cargo data before sending
4. **Cache results** - Store optimization results to reduce API calls
5. **Use webhooks** - For payment events, use webhooks instead of polling
6. **Error handling** - Always check response status and handle errors
7. **HTTPS only** - Never use HTTP for API calls
8. **Monitor usage** - Track your API usage against daily limits

---

## Support

- **Documentation:** https://docs.cargoforge.com
- **Email:** api@cargoforge.com
- **GitHub Issues:** https://github.com/yourusername/CargoForge-C/issues
- **Status Page:** https://status.cargoforge.com

---

**Last Updated:** 2025-11-16
**API Version:** 1.0.0
