# ML Microservice for IoT Wearable Telemetry

An ML microservice built with Flask and scikit-learn for processing and analyzing IoT wearable telemetry data.

## Features
- **High-Risk Hours**: Identifies risk clusters based on hour, intensity, and duration using KMeans clustering.
- **Environmental Exposure**: Calculates rolling baseline using Exponential Weighted Moving Average (EWMA) on medium priority packets.
- **Directional Risk Zones**: Predicts risk direction features based on exposure baselines using a DecisionTreeClassifier.
- **User Interaction Patterns**: Tracks high-priority event ratios, avg confidences, and time between high alerts.
- **Alert Frequency Monitoring**: Highlights anomalies via aggregate daily feature vectors using IsolationForest.

---

## Running Locally

### Prerequisites
- Python 3.11+
- `pip`

### 1. Setup Virtual Environment
```bash
# Create a virtual environment
python -m venv venv

# Activate on Windows:
.\venv\Scripts\activate

# Activate on macOS/Linux:
source venv/bin/activate
```

### 2. Install Dependencies
```bash
pip install -r requirements.txt
```

### 3. Run the Microservice
```bash
# Starts the Flask development server on http://localhost:5000
python app.py
```

---

## Running by Docker

To run the application inside an isolated Docker container:

### Prerequisites
- Docker Engine installed.

### 1. Build the Docker Image
Navigate to this directory (where `Dockerfile` and `requirements.txt` are located) and run:
```bash
docker build -t sense360-ml-microservice .
```

### 2. Run the Container
```bash
# Runs the container and exposes it on port 5000
docker run -p 5000:5000 --name ml-server sense360-ml-microservice
```

---

##  Testing the API

Once running (either natively or via Docker at `http://localhost:5000`), you can post telemetry packets to the ingestion endpoint:

### Endpoint: `POST /api/v1/telemetry`

**Example Request:**
```bash
curl -X POST http://localhost:5000/api/v1/telemetry \
     -H "Content-Type: application/json" \
     -d '{
           "device_id": "STM32_A1",
           "event_id": 10245,
           "event_time": 4521332,
           "intensity": 0.78,
           "duration_ms": 820,
           "direction": "RIGHT",
           "priority": "HIGH",
           "confidence": 0.86
         }'
```

The system will start responding with `null` ML cluster/prediction results until it receives `MIN_SAMPLES_FOR_TRAINING` (default 50) records, at which point it automatically retrains the relevant models and starts returning actual inference results!
