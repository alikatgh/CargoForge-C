#!/usr/bin/env python3
"""
CargoForge-C Production Backend with Authentication & Database
"""

from flask import Flask, request, jsonify, send_from_directory, session, send_file
from flask_cors import CORS
from flask_sqlalchemy import SQLAlchemy
from flask_login import LoginManager, UserMixin, login_user, logout_user, login_required, current_user
from flask_bcrypt import Bcrypt
from flask_limiter import Limiter
from flask_limiter.util import get_remote_address
from datetime import datetime, timedelta
import subprocess
import json
import os
import tempfile
import uuid
import secrets
from functools import wraps
from pdf_export import generate_pdf_report
from stripe_integration import StripeService, WebhookHandler, get_tier_info
import stripe

# Initialize Flask app
app = Flask(__name__, static_folder='../frontend', template_folder='../templates')

# Configuration
app.config['SECRET_KEY'] = os.getenv('SECRET_KEY', secrets.token_hex(32))
app.config['SQLALCHEMY_DATABASE_URI'] = os.getenv('DATABASE_URL', 'sqlite:///cargoforge.db')
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['PERMANENT_SESSION_LIFETIME'] = timedelta(days=7)

# Initialize extensions
db = SQLAlchemy(app)
bcrypt = Bcrypt(app)
login_manager = LoginManager(app)
login_manager.login_view = 'login'
CORS(app, supports_credentials=True)

# Rate limiting
limiter = Limiter(
    app=app,
    key_func=get_remote_address,
    default_limits=["200 per day", "50 per hour"],
    storage_uri="memory://"
)

# Path to C binary
CARGOFORGE_BIN = os.path.join(os.path.dirname(__file__), '../../cargoforge')

# ===== DATABASE MODELS =====

class User(UserMixin, db.Model):
    __tablename__ = 'users'

    id = db.Column(db.Integer, primary_key=True)
    email = db.Column(db.String(120), unique=True, nullable=False, index=True)
    username = db.Column(db.String(80), unique=True, nullable=False, index=True)
    password_hash = db.Column(db.String(255), nullable=False)
    subscription_tier = db.Column(db.String(20), default='free')  # free, pro, enterprise
    subscription_status = db.Column(db.String(20), default='active')  # active, past_due, canceled, canceling
    stripe_customer_id = db.Column(db.String(100), unique=True, nullable=True, index=True)
    stripe_subscription_id = db.Column(db.String(100), unique=True, nullable=True)
    api_key = db.Column(db.String(64), unique=True, nullable=True, index=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    last_login = db.Column(db.DateTime)

    scenarios = db.relationship('Scenario', backref='user', lazy=True, cascade='all, delete-orphan')

    def set_password(self, password):
        self.password_hash = bcrypt.generate_password_hash(password).decode('utf-8')

    def check_password(self, password):
        return bcrypt.check_password_hash(self.password_hash, password)

    def generate_api_key(self):
        self.api_key = secrets.token_hex(32)
        return self.api_key

    @property
    def daily_limit(self):
        limits = {'free': 10, 'pro': 1000, 'enterprise': 10000}
        return limits.get(self.subscription_tier, 10)

class Scenario(db.Model):
    __tablename__ = 'scenarios'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    name = db.Column(db.String(200), nullable=False)
    description = db.Column(db.Text)
    ship_config = db.Column(db.JSON, nullable=False)
    cargo_manifest = db.Column(db.JSON, nullable=False)
    results = db.Column(db.JSON)
    share_token = db.Column(db.String(64), unique=True, index=True)
    is_public = db.Column(db.Boolean, default=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    def generate_share_token(self):
        self.share_token = secrets.token_urlsafe(32)
        return self.share_token

class OptimizationLog(db.Model):
    __tablename__ = 'optimization_logs'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=True)
    scenario_id = db.Column(db.Integer, db.ForeignKey('scenarios.id'), nullable=True)
    execution_time_ms = db.Column(db.Float)
    cargo_count = db.Column(db.Integer)
    placed_count = db.Column(db.Integer)
    success = db.Column(db.Boolean)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

# ===== AUTHENTICATION =====

@login_manager.user_loader
def load_user(user_id):
    return User.query.get(int(user_id))

def api_key_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        api_key = request.headers.get('X-API-Key')
        if not api_key:
            return jsonify({'error': 'API key required'}), 401

        user = User.query.filter_by(api_key=api_key).first()
        if not user:
            return jsonify({'error': 'Invalid API key'}), 401

        # Check daily limit
        today_start = datetime.utcnow().replace(hour=0, minute=0, second=0, microsecond=0)
        today_count = OptimizationLog.query.filter(
            OptimizationLog.user_id == user.id,
            OptimizationLog.created_at >= today_start
        ).count()

        if today_count >= user.daily_limit:
            return jsonify({'error': 'Daily limit exceeded', 'limit': user.daily_limit}), 429

        request.current_user = user
        return f(*args, **kwargs)

    return decorated_function

# ===== HELPER FUNCTIONS =====

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

def run_optimization(ship_data, cargo_list):
    """Run C binary optimization and return results"""
    temp_dir = tempfile.mkdtemp()
    ship_file = os.path.join(temp_dir, f'ship_{uuid.uuid4().hex[:8]}.cfg')
    cargo_file = os.path.join(temp_dir, f'cargo_{uuid.uuid4().hex[:8]}.txt')

    try:
        write_ship_config(ship_data, ship_file)
        write_cargo_manifest(cargo_list, cargo_file)

        import time
        start_time = time.time()

        result = subprocess.run(
            [CARGOFORGE_BIN, ship_file, cargo_file, '--json'],
            capture_output=True,
            text=True,
            timeout=30
        )

        execution_time = (time.time() - start_time) * 1000  # ms

        if result.returncode != 0:
            return None, execution_time, result.stderr

        json_start = result.stdout.find('{')
        if json_start == -1:
            return None, execution_time, 'No JSON output'

        json_output = result.stdout[json_start:]
        response_data = json.loads(json_output)
        response_data['execution'] = {
            'success': True,
            'execution_time_ms': execution_time,
            'warnings': result.stderr.strip().split('\n') if result.stderr else []
        }

        return response_data, execution_time, None

    finally:
        try:
            os.remove(ship_file)
            os.remove(cargo_file)
            os.rmdir(temp_dir)
        except:
            pass

# ===== ROUTES =====

@app.route('/')
def index():
    """Serve landing page"""
    return send_from_directory(app.static_folder, 'landing.html')

@app.route('/app')
def app_page():
    """Serve main application"""
    return send_from_directory(app.static_folder, 'index.html')

@app.route('/dashboard')
def dashboard_page():
    """Serve user dashboard"""
    return send_from_directory(app.static_folder, 'dashboard.html')

# Analytics endpoint
@app.route('/api/analytics', methods=['GET'])
@login_required
def get_analytics():
    """Get user analytics and usage statistics"""
    try:
        from sqlalchemy import func
        from datetime import date

        # Get today's usage count
        today_count = OptimizationLog.query.filter(
            OptimizationLog.user_id == current_user.id,
            func.date(OptimizationLog.created_at) == date.today()
        ).count()

        # Get total optimizations
        total_count = OptimizationLog.query.filter_by(user_id=current_user.id).count()

        # Get average execution time
        avg_time = db.session.query(func.avg(OptimizationLog.execution_time_ms)).filter(
            OptimizationLog.user_id == current_user.id
        ).scalar() or 0

        # Get success rate
        success_count = OptimizationLog.query.filter_by(
            user_id=current_user.id,
            success=True
        ).count()
        success_rate = (success_count / total_count * 100) if total_count > 0 else 0

        # Get scenario count
        scenario_count = Scenario.query.filter_by(user_id=current_user.id).count()

        return jsonify({
            'today_usage': today_count,
            'total_optimizations': total_count,
            'avg_execution_time_ms': round(avg_time, 2),
            'success_rate': round(success_rate, 2),
            'scenario_count': scenario_count,
            'daily_limit': current_user.daily_limit,
            'usage_percentage': (today_count / current_user.daily_limit * 100) if current_user.daily_limit > 0 else 0
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

# Authentication routes
@app.route('/api/auth/register', methods=['POST'])
@limiter.limit("5 per hour")
def register():
    """Register new user"""
    data = request.json

    if not data or not data.get('email') or not data.get('password'):
        return jsonify({'error': 'Email and password required'}), 400

    if User.query.filter_by(email=data['email']).first():
        return jsonify({'error': 'Email already registered'}), 400

    username = data.get('username', data['email'].split('@')[0])
    if User.query.filter_by(username=username).first():
        username = f"{username}_{secrets.token_hex(4)}"

    user = User(email=data['email'], username=username)
    user.set_password(data['password'])
    user.generate_api_key()

    db.session.add(user)
    db.session.commit()

    login_user(user, remember=True)

    return jsonify({
        'success': True,
        'user': {
            'id': user.id,
            'email': user.email,
            'username': user.username,
            'subscription_tier': user.subscription_tier,
            'api_key': user.api_key
        }
    })

@app.route('/api/auth/login', methods=['POST'])
@limiter.limit("10 per hour")
def login():
    """User login"""
    data = request.json

    if not data or not data.get('email') or not data.get('password'):
        return jsonify({'error': 'Email and password required'}), 400

    user = User.query.filter_by(email=data['email']).first()

    if not user or not user.check_password(data['password']):
        return jsonify({'error': 'Invalid credentials'}), 401

    user.last_login = datetime.utcnow()
    db.session.commit()

    login_user(user, remember=data.get('remember', True))

    return jsonify({
        'success': True,
        'user': {
            'id': user.id,
            'email': user.email,
            'username': user.username,
            'subscription_tier': user.subscription_tier
        }
    })

@app.route('/api/auth/logout', methods=['POST'])
@login_required
def logout():
    """User logout"""
    logout_user()
    return jsonify({'success': True})

@app.route('/api/auth/me', methods=['GET'])
@login_required
def get_current_user():
    """Get current user info"""
    return jsonify({
        'user': {
            'id': current_user.id,
            'email': current_user.email,
            'username': current_user.username,
            'subscription_tier': current_user.subscription_tier,
            'api_key': current_user.api_key,
            'daily_limit': current_user.daily_limit
        }
    })

# Scenario management
@app.route('/api/scenarios', methods=['GET'])
@login_required
def get_scenarios():
    """Get user's saved scenarios"""
    scenarios = Scenario.query.filter_by(user_id=current_user.id)\
        .order_by(Scenario.updated_at.desc()).all()

    return jsonify({
        'scenarios': [{
            'id': s.id,
            'name': s.name,
            'description': s.description,
            'created_at': s.created_at.isoformat(),
            'updated_at': s.updated_at.isoformat(),
            'is_public': s.is_public,
            'share_token': s.share_token
        } for s in scenarios]
    })

@app.route('/api/scenarios', methods=['POST'])
@login_required
@limiter.limit("20 per hour")
def create_scenario():
    """Save new scenario"""
    data = request.json

    if not data or not data.get('name'):
        return jsonify({'error': 'Scenario name required'}), 400

    scenario = Scenario(
        user_id=current_user.id,
        name=data['name'],
        description=data.get('description', ''),
        ship_config=data.get('ship', {}),
        cargo_manifest=data.get('cargo', []),
        results=data.get('results')
    )

    if data.get('is_public'):
        scenario.is_public = True
        scenario.generate_share_token()

    db.session.add(scenario)
    db.session.commit()

    return jsonify({
        'success': True,
        'scenario': {
            'id': scenario.id,
            'name': scenario.name,
            'share_token': scenario.share_token
        }
    })

@app.route('/api/scenarios/<int:scenario_id>', methods=['GET'])
@login_required
def get_scenario(scenario_id):
    """Get specific scenario"""
    scenario = Scenario.query.get_or_404(scenario_id)

    if scenario.user_id != current_user.id and not scenario.is_public:
        return jsonify({'error': 'Unauthorized'}), 403

    return jsonify({
        'scenario': {
            'id': scenario.id,
            'name': scenario.name,
            'description': scenario.description,
            'ship': scenario.ship_config,
            'cargo': scenario.cargo_manifest,
            'results': scenario.results,
            'created_at': scenario.created_at.isoformat()
        }
    })

@app.route('/api/scenarios/<int:scenario_id>', methods=['DELETE'])
@login_required
def delete_scenario(scenario_id):
    """Delete scenario"""
    scenario = Scenario.query.get_or_404(scenario_id)

    if scenario.user_id != current_user.id:
        return jsonify({'error': 'Unauthorized'}), 403

    db.session.delete(scenario)
    db.session.commit()

    return jsonify({'success': True})

# Optimization endpoint
@app.route('/api/optimize', methods=['POST'])
@limiter.limit("30 per minute")
def optimize():
    """Optimize cargo placement"""
    try:
        data = request.json

        if not data or 'ship' not in data or 'cargo' not in data:
            return jsonify({'error': 'Missing ship or cargo data'}), 400

        # Run optimization
        result, exec_time, error = run_optimization(data['ship'], data['cargo'])

        if error:
            return jsonify({'error': f'Optimization failed: {error}'}), 500

        # Log the optimization
        if current_user.is_authenticated:
            log = OptimizationLog(
                user_id=current_user.id,
                execution_time_ms=exec_time,
                cargo_count=len(data['cargo']),
                placed_count=result['analysis']['placed_count'],
                success=True
            )
            db.session.add(log)
            db.session.commit()

        return jsonify(result)

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/challenges', methods=['GET'])
def get_challenges():
    """Get training challenges"""
    challenges = [
        {
            'id': 1,
            'title': 'Getting Started',
            'difficulty': 'beginner',
            'description': 'Learn the basics by loading 3 containers on a small vessel',
            'ship': {'length': 100, 'width': 20, 'max_weight': 10000000},
            'cargo': [
                {'id': 'Container1', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'standard'},
                {'id': 'Container2', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'standard'},
                {'id': 'Container3', 'weight': 200000, 'dimensions': [10, 2.4, 2.6], 'type': 'standard'}
            ],
            'goals': {'cg_range': [40, 60], 'min_gm': 0.5, 'all_placed': True}
        },
        {
            'id': 2,
            'title': 'Balance Master',
            'difficulty': 'intermediate',
            'description': 'Achieve perfect longitudinal balance with asymmetric cargo',
            'ship': {'length': 150, 'width': 25, 'max_weight': 50000000},
            'cargo': [
                {'id': 'HeavyFwd', 'weight': 800000, 'dimensions': [15, 5, 4], 'type': 'heavy'},
                {'id': 'LightAft1', 'weight': 200000, 'dimensions': [8, 3, 2], 'type': 'standard'},
                {'id': 'LightAft2', 'weight': 200000, 'dimensions': [8, 3, 2], 'type': 'standard'},
                {'id': 'MidWeight', 'weight': 400000, 'dimensions': [10, 4, 3], 'type': 'standard'}
            ],
            'goals': {'cg_range': [48, 52], 'balance_status': 'good', 'stability_status': 'optimal'}
        },
        {
            'id': 3,
            'title': 'Hazmat Separation',
            'difficulty': 'advanced',
            'description': 'Safely load hazardous materials with proper separation',
            'ship': {'length': 200, 'width': 30, 'max_weight': 80000000},
            'cargo': [
                {'id': 'Hazmat1', 'weight': 300000, 'dimensions': [10, 3, 3], 'type': 'hazardous'},
                {'id': 'Hazmat2', 'weight': 300000, 'dimensions': [10, 3, 3], 'type': 'hazardous'},
                {'id': 'Reefer1', 'weight': 250000, 'dimensions': [12, 2.4, 2.6], 'type': 'reefer'},
                {'id': 'General1', 'weight': 400000, 'dimensions': [15, 4, 3], 'type': 'standard'},
                {'id': 'General2', 'weight': 400000, 'dimensions': [15, 4, 3], 'type': 'standard'}
            ],
            'goals': {'all_placed': True, 'stability_status': 'optimal', 'no_constraint_violations': True}
        }
    ]

    return jsonify({'challenges': challenges})

# PDF Export endpoint
@app.route('/api/export/pdf', methods=['POST'])
@login_required
@limiter.limit("10 per hour")
def export_pdf():
    """Export optimization results as PDF"""
    try:
        data = request.json

        if not data or 'ship' not in data or 'cargo' not in data:
            return jsonify({'error': 'Missing ship or cargo data'}), 400

        # Run optimization to get current results
        result, exec_time, error = run_optimization(data['ship'], data['cargo'])

        if error:
            return jsonify({'error': f'Optimization failed: {error}'}), 500

        # Prepare data for PDF
        optimization_data = {
            'ship': data['ship'],
            'cargo': result['cargo'],
            'analysis': result['analysis']
        }

        # Get scenario name from request or use default
        scenario_name = data.get('scenario_name', 'Untitled Scenario')

        # Generate PDF
        pdf_buffer = generate_pdf_report(
            optimization_data,
            user_tier=current_user.subscription_tier,
            scenario_name=scenario_name
        )

        # Log the export
        log = OptimizationLog(
            user_id=current_user.id,
            execution_time_ms=exec_time,
            cargo_count=len(data['cargo']),
            placed_count=result['analysis']['placed_count'],
            success=True
        )
        db.session.add(log)
        db.session.commit()

        # Send PDF file
        return send_file(
            pdf_buffer,
            mimetype='application/pdf',
            as_attachment=True,
            download_name=f'cargo_loading_plan_{scenario_name.replace(" ", "_")}.pdf'
        )

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/export/pdf/<int:scenario_id>', methods=['GET'])
@login_required
@limiter.limit("10 per hour")
def export_scenario_pdf(scenario_id):
    """Export saved scenario as PDF"""
    try:
        # Get scenario
        scenario = Scenario.query.filter_by(id=scenario_id).first()

        if not scenario:
            return jsonify({'error': 'Scenario not found'}), 404

        # Check ownership or public access
        if scenario.user_id != current_user.id and not scenario.is_public:
            return jsonify({'error': 'Access denied'}), 403

        # Get optimization results
        if scenario.results:
            optimization_data = {
                'ship': scenario.ship_config,
                'cargo': scenario.results.get('cargo', []),
                'analysis': scenario.results.get('analysis', {})
            }
        else:
            # Run optimization if no cached results
            result, _, error = run_optimization(scenario.ship_config, scenario.cargo_manifest)
            if error:
                return jsonify({'error': f'Optimization failed: {error}'}), 500

            optimization_data = {
                'ship': scenario.ship_config,
                'cargo': result['cargo'],
                'analysis': result['analysis']
            }

        # Generate PDF
        pdf_buffer = generate_pdf_report(
            optimization_data,
            user_tier=current_user.subscription_tier,
            scenario_name=scenario.name
        )

        # Send PDF file
        return send_file(
            pdf_buffer,
            mimetype='application/pdf',
            as_attachment=True,
            download_name=f'cargo_loading_plan_{scenario.name.replace(" ", "_")}.pdf'
        )

    except Exception as e:
        return jsonify({'error': str(e)}), 500

# ===== BILLING & PAYMENTS =====

@app.route('/api/billing/pricing', methods=['GET'])
def get_pricing():
    """Get pricing tier information"""
    return jsonify(get_tier_info())

@app.route('/api/billing/checkout', methods=['POST'])
@login_required
@limiter.limit("5 per hour")
def create_checkout():
    """Create Stripe checkout session for subscription purchase"""
    try:
        data = request.json
        tier = data.get('tier')  # 'pro_monthly' or 'pro_annual'

        if not tier:
            return jsonify({'error': 'Tier required'}), 400

        if tier not in ['pro_monthly', 'pro_annual']:
            return jsonify({'error': 'Invalid tier'}), 400

        # Check if already subscribed
        if current_user.subscription_tier != 'free':
            return jsonify({'error': 'Already subscribed. Use billing portal to manage subscription.'}), 400

        # Create checkout session
        success_url = request.host_url + 'app?payment=success'
        cancel_url = request.host_url + 'app?payment=canceled'

        session = StripeService.create_checkout_session(
            user_email=current_user.email,
            tier=tier,
            success_url=success_url,
            cancel_url=cancel_url
        )

        return jsonify({
            'checkout_url': session.url,
            'session_id': session.id
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/billing/portal', methods=['POST'])
@login_required
@limiter.limit("10 per hour")
def create_portal_session():
    """Create Stripe customer portal session for subscription management"""
    try:
        if not current_user.stripe_customer_id:
            return jsonify({'error': 'No active subscription'}), 400

        return_url = request.host_url + 'app'

        session = StripeService.create_customer_portal_session(
            customer_id=current_user.stripe_customer_id,
            return_url=return_url
        )

        return jsonify({
            'portal_url': session.url
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/billing/cancel', methods=['POST'])
@login_required
@limiter.limit("5 per hour")
def cancel_subscription():
    """Cancel subscription at period end"""
    try:
        if not current_user.stripe_subscription_id:
            return jsonify({'error': 'No active subscription'}), 400

        subscription = StripeService.cancel_subscription(current_user.stripe_subscription_id)

        # Update user status
        current_user.subscription_status = 'canceling'
        db.session.commit()

        return jsonify({
            'success': True,
            'message': 'Subscription will be cancelled at period end',
            'period_end': subscription.current_period_end
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/billing/reactivate', methods=['POST'])
@login_required
@limiter.limit("5 per hour")
def reactivate_subscription():
    """Reactivate a cancelled subscription"""
    try:
        if not current_user.stripe_subscription_id:
            return jsonify({'error': 'No subscription to reactivate'}), 400

        subscription = StripeService.reactivate_subscription(current_user.stripe_subscription_id)

        # Update user status
        current_user.subscription_status = 'active'
        db.session.commit()

        return jsonify({
            'success': True,
            'message': 'Subscription reactivated successfully'
        })

    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/billing/webhook', methods=['POST'])
@limiter.exempt
def stripe_webhook():
    """Handle Stripe webhook events"""
    payload = request.data
    signature = request.headers.get('Stripe-Signature')

    try:
        # Construct and verify event
        event = StripeService.construct_webhook_event(payload, signature)

        # Handle different event types
        event_type = event['type']
        event_data = event['data']['object']

        if event_type == 'checkout.session.completed':
            WebhookHandler.handle_checkout_completed(event_data, db, User)

        elif event_type == 'invoice.payment_succeeded':
            WebhookHandler.handle_invoice_paid(event_data, db, User)

        elif event_type == 'invoice.payment_failed':
            WebhookHandler.handle_invoice_failed(event_data, db, User)

        elif event_type == 'customer.subscription.updated':
            WebhookHandler.handle_subscription_updated(event_data, db, User)

        elif event_type == 'customer.subscription.deleted':
            WebhookHandler.handle_subscription_deleted(event_data, db, User)

        return jsonify({'success': True}), 200

    except Exception as e:
        print(f"Webhook error: {str(e)}")
        return jsonify({'error': str(e)}), 400

# Initialize database
with app.app_context():
    db.create_all()

if __name__ == '__main__':
    if not os.path.exists(CARGOFORGE_BIN):
        print(f"ERROR: CargoForge binary not found at {CARGOFORGE_BIN}")
        print("Please build the project first: make")
        exit(1)

    print("CargoForge Production API starting...")
    print(f"Database: {app.config['SQLALCHEMY_DATABASE_URI']}")
    print(f"Binary: {CARGOFORGE_BIN}")
    print("Access: http://localhost:5000")

    app.run(debug=True, host='0.0.0.0', port=5000)
