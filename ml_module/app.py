import logging
from flask import Flask, request, jsonify
from pydantic import ValidationError

from config import Config, setup_logging
from models import TelemetryEvent
from ml_engine import MLEngine

setup_logging()
logger = logging.getLogger(__name__)

app = Flask(__name__)
ml_engine = MLEngine()

@app.route("/health", methods=["GET"])
def health_check():
    """Service health endpoint."""
    return jsonify({"status": "healthy"}), 200

@app.route("/api/v1/telemetry", methods=["POST"])
def ingest_telemetry():
    """
    Ingests telemetry records, processes them through the ML pipeline,
    and returns analytical insights.
    """
    try:
        # Validate incoming JSON against the expected schema
        payload = request.get_json(force=True)
        event = TelemetryEvent(**payload)
        
        # Process the event and generate predictions/analytics
        analytics = ml_engine.process_event(event)
        
        return jsonify({
            "status": "success",
            "event_id": event.event_id,
            "analytics": analytics
        }), 200

    except ValidationError as e:
        logger.warning(f"Data validation error: {e.errors()}")
        return jsonify({
            "status": "error",
            "message": "Invalid payload format",
            "details": e.errors()
        }), 400
        
    except Exception as e:
        logger.error("Internal server error during event processing", exc_info=True)
        return jsonify({
            "status": "error",
            "message": "Internal server error processing event"
        }), 500

if __name__ == "__main__":
    logger.info("Starting ML microservice...")
    app.run(host=Config.HOST, port=Config.PORT, debug=Config.DEBUG)
