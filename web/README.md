# CargoForge-C Web Interface

Interactive web-based UI for the CargoForge-C maritime cargo simulator.

## Features

- **3D Visualization**: Interactive Three.js rendering of ship and cargo
- **Training Challenges**: Progressive difficulty levels to learn cargo loading
- **Real-time Optimization**: Fast backend powered by C binary
- **Responsive Design**: Works on desktop and tablet devices

## Quick Start

### 1. Build the C Binary

```bash
cd ..
make
```

### 2. Install Python Dependencies

```bash
cd web/backend
pip install -r requirements.txt
```

### 3. Run the Web Server

```bash
python app.py
```

### 4. Open in Browser

Navigate to: `http://localhost:5000`

## Architecture

```
web/
├── backend/
│   ├── app.py              # Flask API server
│   └── requirements.txt    # Python dependencies
├── frontend/
│   ├── index.html         # Main web interface
│   ├── app.js             # Application logic
│   └── visualizer.js      # Three.js 3D visualization
└── README.md
```

## API Endpoints

### POST /api/optimize
Optimize cargo placement for a given ship and cargo manifest.

**Request:**
```json
{
  "ship": {
    "length": 150,
    "width": 25,
    "max_weight": 50000000
  },
  "cargo": [
    {
      "id": "Container1",
      "weight": 250000,
      "dimensions": [12, 2.4, 2.6],
      "type": "standard"
    }
  ]
}
```

**Response:**
```json
{
  "ship": {...},
  "cargo": [...],
  "analysis": {
    "placed_count": 5,
    "stability_status": "optimal",
    "metacentric_height": 1.85,
    ...
  }
}
```

### GET /api/challenges
Get list of training challenges.

### POST /api/validate
Validate ship/cargo configuration without running optimization.

## Development

### Debugging

Enable Flask debug mode (already enabled in app.py):
```python
app.run(debug=True, host='0.0.0.0', port=5000)
```

View browser console for JavaScript errors.

### Adding Challenges

Edit the challenges array in `backend/app.py`:

```python
challenges = [
    {
        'id': 4,
        'title': 'Your Challenge',
        'difficulty': 'beginner',
        'description': '...',
        'ship': {...},
        'cargo': [...],
        'goals': {...}
    }
]
```

## Deployment

### Docker (Production)

```bash
# Build image
docker build -t cargoforge-web .

# Run container
docker run -p 5000:5000 cargoforge-web
```

### Cloud Deployment

Deploy to Heroku, Railway, or any platform supporting Python Flask apps:

1. Add `Procfile`:
   ```
   web: cd web/backend && gunicorn app:app
   ```

2. Add `runtime.txt`:
   ```
   python-3.11
   ```

3. Deploy using platform CLI

## Browser Support

- Chrome 90+
- Firefox 88+
- Safari 14+
- Edge 90+

Requires WebGL support for 3D visualization.
