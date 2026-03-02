import os
import logging

class Config:
    # API configuration
    HOST = os.getenv("API_HOST", "0.0.0.0")
    PORT = int(os.getenv("API_PORT", 5000))
    DEBUG = os.getenv("API_DEBUG", "False").lower() in ("true", "1", "t")

    # ML Configuration
    MIN_SAMPLES_FOR_TRAINING = int(os.getenv("MIN_SAMPLES_FOR_TRAINING", 50))
    KMEANS_N_CLUSTERS = int(os.getenv("KMEANS_N_CLUSTERS", 3))
    EWMA_ALPHA = float(os.getenv("EWMA_ALPHA", 0.1))
    ISOLATION_FOREST_CONTAMINATION = float(os.getenv("ISOLATION_FOREST_CONTAMINATION", 0.05))

    # Logging config
    LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")

def setup_logging():
    logging.basicConfig(
        level=getattr(logging, Config.LOG_LEVEL),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
    )
