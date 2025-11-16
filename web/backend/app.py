#!/usr/bin/env python3
"""
CargoForge-C Web API Backend
Flask API wrapper around the C cargo optimizer binary
"""

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import subprocess
import json
import os
import tempfile
import uuid

app = Flask(__name__, static_folder='../frontend', template_folder='../templates')
CORS(app)  # Enable CORS for development

# Path to the C binary
CARGOFORGE_BIN = os.path.join(os.path.dirname(__file__), '../../cargoforge')

def write_ship_config(ship_data, filepath):
    """Write ship configuration to file"""
    with open(filepath, 'w') as f:
        f.write(f"length_m={ship_data['length']}\n")
        f.write(f"width_m={ship_data['width']}\n")
        f.write(f"max_weight_kg={ship_data['max_weight']}\n")
        f.write(f"lightship_weight_kg={ship_data.get('lightship_weight', ship_data['max_weight'] * 0.2)}\n")
        f.write(f"lightship_kg_m={ship_data.get('lightship_kg', 8.0)}\n")

def write_cargo_manifest(cargo_list, filepath):
    """Write cargo manifest to file"""
    with open(filepath, 'w') as f:
        f.write("# id weight_kg length_m width_m height_m type\n")
        for cargo in cargo_list:
            f.write(f"{cargo['id']} {cargo['weight']} "
                   f"{cargo['dimensions'][0]} {cargo['dimensions'][1]} {cargo['dimensions'][2]} "
                   f"{cargo.get('type', 'standard')}\n")

@app.route('/')
def index():
    """Serve the main web interface"""
    return send_from_directory(app.static_folder, 'index.html')

@app.route('/api/optimize', methods=['POST'])
def optimize():
    """
    Optimize cargo placement

    Request JSON:
    {
        "ship": {"length": 150, "width": 25, "max_weight": 50000000},
        "cargo": [{"id": "C1", "weight": 500000, "dimensions": [10, 5, 3], "type": "standard"}, ...]
    }
    """
    try:
        data = request.json

        # Validate input
        if not data or 'ship' not in data or 'cargo' not in data:
            return jsonify({'error': 'Missing ship or cargo data'}), 400

        # Create temporary files
        temp_dir = tempfile.mkdtemp()
        ship_file = os.path.join(temp_dir, f'ship_{uuid.uuid4().hex[:8]}.cfg')
        cargo_file = os.path.join(temp_dir, f'cargo_{uuid.uuid4().hex[:8]}.txt')

        try:
            write_ship_config(data['ship'], ship_file)
            write_cargo_manifest(data['cargo'], cargo_file)

            # Run the C binary
            result = subprocess.run(
                [CARGOFORGE_BIN, ship_file, cargo_file, '--json'],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode != 0:
                return jsonify({'error': f'Optimization failed: {result.stderr}'}), 500

            # Parse JSON output (filter stderr warnings)
            json_start = result.stdout.find('{')
            if json_start == -1:
                return jsonify({'error': 'No JSON output from optimizer'}), 500

            json_output = result.stdout[json_start:]
            response_data = json.loads(json_output)

            # Add execution info
            response_data['execution'] = {
                'success': True,
                'warnings': result.stderr.strip().split('\n') if result.stderr else []
            }

            return jsonify(response_data)

        finally:
            # Cleanup temp files
            try:
                os.remove(ship_file)
                os.remove(cargo_file)
                os.rmdir(temp_dir)
            except:
                pass

    except subprocess.TimeoutExpired:
        return jsonify({'error': 'Optimization timeout (>30s)'}), 408
    except json.JSONDecodeError as e:
        return jsonify({'error': f'Invalid JSON output: {str(e)}'}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/challenges', methods=['GET'])
def get_challenges():
    """Get tutorial challenges"""
    challenges = [
        {
            'id': 1,
            'title': 'Getting Started',
            'difficulty': 'beginner',
            'description': 'Learn the basics by loading 3 containers on a small vessel',
            'ship': {
                'length': 100,
                'width': 20,
                'max_weight': 10000000
            },
            'cargo': [
                {'id': 'Container1', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'standard'},
                {'id': 'Container2', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'standard'},
                {'id': 'Container3', 'weight': 200000, 'dimensions': [10, 2.4, 2.6], 'type': 'standard'}
            ],
            'goals': {
                'cg_range': [40, 60],
                'min_gm': 0.5,
                'all_placed': True
            }
        },
        {
            'id': 2,
            'title': 'Balance Master',
            'difficulty': 'intermediate',
            'description': 'Achieve perfect longitudinal balance with asymmetric cargo',
            'ship': {
                'length': 150,
                'width': 25,
                'max_weight': 50000000
            },
            'cargo': [
                {'id': 'HeavyFwd', 'weight': 800000, 'dimensions': [15, 5, 4], 'type': 'heavy'},
                {'id': 'LightAft1', 'weight': 200000, 'dimensions': [8, 3, 2], 'type': 'standard'},
                {'id': 'LightAft2', 'weight': 200000, 'dimensions': [8, 3, 2], 'type': 'standard'},
                {'id': 'MidWeight', 'weight': 400000, 'dimensions': [10, 4, 3], 'type': 'standard'}
            ],
            'goals': {
                'cg_range': [48, 52],
                'balance_status': 'good',
                'stability_status': 'optimal'
            }
        },
        {
            'id': 3,
            'title': 'Hazmat Separation',
            'difficulty': 'advanced',
            'description': 'Safely load hazardous materials with proper separation',
            'ship': {
                'length': 200,
                'width': 30,
                'max_weight': 80000000
            },
            'cargo': [
                {'id': 'Hazmat1', 'weight': 300000, 'dimensions': [10, 3, 3], 'type': 'hazardous'},
                {'id': 'Hazmat2', 'weight': 300000, 'dimensions': [10, 3, 3], 'type': 'hazardous'},
                {'id': 'Reefer1', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'reefer'},
                {'id': 'General1', 'weight': 400000, 'dimensions': [15, 4, 3], 'type': 'standard'},
                {'id': 'General2', 'weight': 400000, 'dimensions': [15, 4, 3], 'type': 'standard'}
            ],
            'goals': {
                'all_placed': True,
                'stability_status': 'optimal',
                'no_constraint_violations': True
            }
        }
    ]

    return jsonify({'challenges': challenges})

@app.route('/api/validate', methods=['POST'])
def validate():
    """Validate ship/cargo configuration without running optimization"""
    try:
        data = request.json
        errors = []

        # Validate ship
        ship = data.get('ship', {})
        if ship.get('length', 0) <= 0:
            errors.append('Ship length must be positive')
        if ship.get('width', 0) <= 0:
            errors.append('Ship width must be positive')
        if ship.get('max_weight', 0) <= 0:
            errors.append('Ship max weight must be positive')

        # Validate cargo
        cargo = data.get('cargo', [])
        if not cargo:
            errors.append('At least one cargo item required')

        for i, item in enumerate(cargo):
            if not item.get('id'):
                errors.append(f'Cargo {i+1}: ID required')
            if item.get('weight', 0) <= 0:
                errors.append(f'Cargo {item.get("id", i+1)}: Weight must be positive')
            dims = item.get('dimensions', [])
            if len(dims) != 3 or any(d <= 0 for d in dims):
                errors.append(f'Cargo {item.get("id", i+1)}: Invalid dimensions')

        return jsonify({
            'valid': len(errors) == 0,
            'errors': errors
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    # Check if binary exists
    if not os.path.exists(CARGOFORGE_BIN):
        print(f"ERROR: CargoForge binary not found at {CARGOFORGE_BIN}")
        print("Please build the project first: make")
        exit(1)

    print("CargoForge Web API starting...")
    print(f"Binary: {CARGOFORGE_BIN}")
    print("Access the web interface at: http://localhost:5000")

    app.run(debug=True, host='0.0.0.0', port=5000)
