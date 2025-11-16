"""
Stripe Payment Integration for CargoForge
Handles subscription billing and payment processing
"""

import stripe
import os
from datetime import datetime


# Initialize Stripe
stripe.api_key = os.getenv('STRIPE_SECRET_KEY', '')
STRIPE_WEBHOOK_SECRET = os.getenv('STRIPE_WEBHOOK_SECRET', '')

# Price IDs (these should be created in Stripe Dashboard)
STRIPE_PRICES = {
    'pro_monthly': os.getenv('STRIPE_PRICE_PRO_MONTHLY', 'price_pro_monthly'),
    'pro_annual': os.getenv('STRIPE_PRICE_PRO_ANNUAL', 'price_pro_annual'),
    'enterprise': 'contact_sales'  # Enterprise is custom pricing
}


class StripeService:
    """Service class for Stripe payment operations"""

    @staticmethod
    def create_checkout_session(user_email, tier, success_url, cancel_url):
        """
        Create a Stripe Checkout Session for subscription purchase

        Args:
            user_email: User's email address
            tier: Subscription tier ('pro_monthly' or 'pro_annual')
            success_url: URL to redirect after successful payment
            cancel_url: URL to redirect if payment is cancelled

        Returns:
            Checkout session object with URL
        """
        if tier not in STRIPE_PRICES or STRIPE_PRICES[tier] == 'contact_sales':
            raise ValueError(f"Invalid tier: {tier}")

        try:
            session = stripe.checkout.Session.create(
                customer_email=user_email,
                payment_method_types=['card'],
                line_items=[{
                    'price': STRIPE_PRICES[tier],
                    'quantity': 1,
                }],
                mode='subscription',
                success_url=success_url + '?session_id={CHECKOUT_SESSION_ID}',
                cancel_url=cancel_url,
                metadata={
                    'tier': tier.split('_')[0],  # 'pro' from 'pro_monthly'
                }
            )
            return session
        except stripe.error.StripeError as e:
            raise Exception(f"Stripe error: {str(e)}")

    @staticmethod
    def create_customer_portal_session(customer_id, return_url):
        """
        Create a Stripe Customer Portal Session for subscription management

        Args:
            customer_id: Stripe customer ID
            return_url: URL to return to after portal session

        Returns:
            Portal session object with URL
        """
        try:
            session = stripe.billing_portal.Session.create(
                customer=customer_id,
                return_url=return_url,
            )
            return session
        except stripe.error.StripeError as e:
            raise Exception(f"Stripe error: {str(e)}")

    @staticmethod
    def cancel_subscription(subscription_id):
        """
        Cancel a subscription at period end

        Args:
            subscription_id: Stripe subscription ID

        Returns:
            Updated subscription object
        """
        try:
            subscription = stripe.Subscription.modify(
                subscription_id,
                cancel_at_period_end=True
            )
            return subscription
        except stripe.error.StripeError as e:
            raise Exception(f"Stripe error: {str(e)}")

    @staticmethod
    def reactivate_subscription(subscription_id):
        """
        Reactivate a cancelled subscription

        Args:
            subscription_id: Stripe subscription ID

        Returns:
            Updated subscription object
        """
        try:
            subscription = stripe.Subscription.modify(
                subscription_id,
                cancel_at_period_end=False
            )
            return subscription
        except stripe.error.StripeError as e:
            raise Exception(f"Stripe error: {str(e)}")

    @staticmethod
    def get_subscription(subscription_id):
        """
        Get subscription details

        Args:
            subscription_id: Stripe subscription ID

        Returns:
            Subscription object
        """
        try:
            subscription = stripe.Subscription.retrieve(subscription_id)
            return subscription
        except stripe.error.StripeError as e:
            raise Exception(f"Stripe error: {str(e)}")

    @staticmethod
    def construct_webhook_event(payload, signature):
        """
        Construct and verify webhook event from Stripe

        Args:
            payload: Raw request body
            signature: Stripe signature header

        Returns:
            Verified event object
        """
        try:
            event = stripe.Webhook.construct_event(
                payload, signature, STRIPE_WEBHOOK_SECRET
            )
            return event
        except ValueError:
            raise Exception("Invalid payload")
        except stripe.error.SignatureVerificationError:
            raise Exception("Invalid signature")


class WebhookHandler:
    """Handle Stripe webhook events"""

    @staticmethod
    def handle_checkout_completed(session, db, User):
        """
        Handle checkout.session.completed event

        Args:
            session: Stripe checkout session
            db: SQLAlchemy database instance
            User: User model class
        """
        customer_email = session.get('customer_email')
        customer_id = session.get('customer')
        subscription_id = session.get('subscription')
        tier = session.get('metadata', {}).get('tier', 'pro')

        # Find user by email
        user = User.query.filter_by(email=customer_email).first()
        if not user:
            print(f"Warning: User not found for email {customer_email}")
            return

        # Update user subscription
        user.subscription_tier = tier
        user.stripe_customer_id = customer_id
        user.stripe_subscription_id = subscription_id
        user.subscription_status = 'active'

        db.session.commit()
        print(f"User {user.email} upgraded to {tier}")

    @staticmethod
    def handle_invoice_paid(invoice, db, User):
        """
        Handle invoice.payment_succeeded event

        Args:
            invoice: Stripe invoice object
            db: SQLAlchemy database instance
            User: User model class
        """
        customer_id = invoice.get('customer')
        subscription_id = invoice.get('subscription')

        # Find user by Stripe customer ID
        user = User.query.filter_by(stripe_customer_id=customer_id).first()
        if not user:
            print(f"Warning: User not found for customer {customer_id}")
            return

        # Update subscription status
        user.subscription_status = 'active'
        user.stripe_subscription_id = subscription_id

        db.session.commit()
        print(f"Invoice paid for user {user.email}")

    @staticmethod
    def handle_invoice_failed(invoice, db, User):
        """
        Handle invoice.payment_failed event

        Args:
            invoice: Stripe invoice object
            db: SQLAlchemy database instance
            User: User model class
        """
        customer_id = invoice.get('customer')

        # Find user by Stripe customer ID
        user = User.query.filter_by(stripe_customer_id=customer_id).first()
        if not user:
            print(f"Warning: User not found for customer {customer_id}")
            return

        # Update subscription status
        user.subscription_status = 'past_due'

        db.session.commit()
        print(f"Payment failed for user {user.email}")

    @staticmethod
    def handle_subscription_updated(subscription, db, User):
        """
        Handle customer.subscription.updated event

        Args:
            subscription: Stripe subscription object
            db: SQLAlchemy database instance
            User: User model class
        """
        customer_id = subscription.get('customer')
        subscription_id = subscription.get('id')
        status = subscription.get('status')
        cancel_at_period_end = subscription.get('cancel_at_period_end', False)

        # Find user by Stripe customer ID
        user = User.query.filter_by(stripe_customer_id=customer_id).first()
        if not user:
            print(f"Warning: User not found for customer {customer_id}")
            return

        # Update subscription status
        user.subscription_status = status
        user.stripe_subscription_id = subscription_id

        # If subscription is cancelled at period end, mark it
        if cancel_at_period_end:
            user.subscription_status = 'canceling'

        db.session.commit()
        print(f"Subscription updated for user {user.email}: {status}")

    @staticmethod
    def handle_subscription_deleted(subscription, db, User):
        """
        Handle customer.subscription.deleted event

        Args:
            subscription: Stripe subscription object
            db: SQLAlchemy database instance
            User: User model class
        """
        customer_id = subscription.get('customer')

        # Find user by Stripe customer ID
        user = User.query.filter_by(stripe_customer_id=customer_id).first()
        if not user:
            print(f"Warning: User not found for customer {customer_id}")
            return

        # Downgrade to free tier
        user.subscription_tier = 'free'
        user.subscription_status = 'canceled'
        user.stripe_subscription_id = None

        db.session.commit()
        print(f"Subscription cancelled for user {user.email}, downgraded to free tier")


def get_tier_info():
    """Get pricing tier information"""
    return {
        'free': {
            'name': 'Free',
            'price': 0,
            'currency': 'USD',
            'interval': None,
            'daily_limit': 10,
            'features': [
                '10 optimizations per day',
                'Basic 3D visualization',
                'Training challenges',
                'Community support'
            ]
        },
        'pro_monthly': {
            'name': 'Pro (Monthly)',
            'price': 29,
            'currency': 'USD',
            'interval': 'month',
            'daily_limit': 1000,
            'stripe_price_id': STRIPE_PRICES['pro_monthly'],
            'features': [
                '1,000 optimizations per day',
                'Save unlimited scenarios',
                'PDF export',
                'API access',
                'Priority support',
                'Advanced analytics'
            ]
        },
        'pro_annual': {
            'name': 'Pro (Annual)',
            'price': 290,  # ~17% discount
            'currency': 'USD',
            'interval': 'year',
            'daily_limit': 1000,
            'stripe_price_id': STRIPE_PRICES['pro_annual'],
            'features': [
                '1,000 optimizations per day',
                'Save unlimited scenarios',
                'PDF export',
                'API access',
                'Priority support',
                'Advanced analytics',
                '2 months free (annual billing)'
            ]
        },
        'enterprise': {
            'name': 'Enterprise',
            'price': 'Custom',
            'currency': 'USD',
            'interval': None,
            'daily_limit': 10000,
            'contact_sales': True,
            'features': [
                'Unlimited optimizations',
                'Custom ship configurations',
                'Dedicated support',
                'SLA guarantee',
                'On-premise deployment option',
                'Custom integrations',
                'White-label solution'
            ]
        }
    }
