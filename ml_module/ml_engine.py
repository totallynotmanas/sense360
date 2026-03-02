import logging
from typing import List, Dict, Optional
from collections import defaultdict
from datetime import datetime
import numpy as np
import pandas as pd
from sklearn.cluster import KMeans
from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble import IsolationForest

from models import TelemetryEvent
from config import Config

logger = logging.getLogger(__name__)

class MLEngine:
    """
    Core Machine Learning engine for IoT wearable telemetry.
    Maintains localized state and provides batch updating and prediction capabilities.
    """
    def __init__(self):
        self._events: List[TelemetryEvent] = []
        
        # Models
        self._kmeans: Optional[KMeans] = None
        self._dt_classifier: Optional[DecisionTreeClassifier] = None
        self._isolation_forest: Optional[IsolationForest] = None
        
        # State variables for continuous analytics
        self._exposure_baseline: float = 0.0
        
        self._daily_metrics = defaultdict(lambda: {"high_count": 0, "total_intensity": 0.0, "total_duration": 0})
        self._last_trained_count = 0

    def process_event(self, event: TelemetryEvent) -> Dict[str, any]:
        """
        Processes a single event, updates active models, runs predictions, and queues data for batch retraining.
        """
        self._events.append(event)
        
        self._update_exposure_baseline(event)
        self._update_daily_metrics(event)

        predictions = self._run_inference(event)
        
        if len(self._events) - self._last_trained_count >= Config.MIN_SAMPLES_FOR_TRAINING:
            self._train_models()
            
        return predictions

    def _update_exposure_baseline(self, event: TelemetryEvent) -> None:
        """Updates the rolling baseline for Environmental Exposure using MEDIUM priority packets."""
        if event.priority == "MEDIUM":
            weighted_intensity = event.intensity * (event.duration_ms / 1000.0)
            self._exposure_baseline = (Config.EWMA_ALPHA * weighted_intensity) + \
                                      ((1 - Config.EWMA_ALPHA) * self._exposure_baseline)

    def _update_daily_metrics(self, event: TelemetryEvent) -> None:
        """Tracks daily metrics for Alert Frequency anomaly detection."""
        event_date = datetime.fromtimestamp(event.event_time / 1000.0).date()
        if event.priority == "HIGH":
            self._daily_metrics[event_date]["high_count"] += 1
            self._daily_metrics[event_date]["total_intensity"] += event.intensity
            self._daily_metrics[event_date]["total_duration"] += event.duration_ms

    def _run_inference(self, event: TelemetryEvent) -> Dict[str, any]:
        """Runs inference against trained models for the incoming event."""
        result = {
            "current_exposure_baseline": round(self._exposure_baseline, 4),
            "high_risk_cluster": None,
            "predicted_direction_risk": None,
            "daily_anomaly": None,
            "interaction_metrics": self._calculate_interaction_metrics()
        }

        hour = datetime.fromtimestamp(event.event_time / 1000.0).hour

        if self._kmeans and event.priority == "HIGH":
            features = np.array([[hour, event.intensity, event.duration_ms]])
            result["high_risk_cluster"] = int(self._kmeans.predict(features)[0])
            
        if self._dt_classifier:
            features = np.array([[hour, event.intensity, event.duration_ms, self._exposure_baseline]])
            result["predicted_direction_risk"] = str(self._dt_classifier.predict(features)[0])

        if self._isolation_forest:
            event_date = datetime.fromtimestamp(event.event_time / 1000.0).date()
            daily_data = self._daily_metrics[event_date]
            count = daily_data["high_count"]
            if count > 0:
                avg_intensity = daily_data["total_intensity"] / count
                avg_duration = daily_data["total_duration"] / count
                features = np.array([[count, avg_intensity, avg_duration]])
                anomaly_score = self._isolation_forest.predict(features)[0]
                result["daily_anomaly"] = bool(anomaly_score == -1)

        return result

    def _calculate_interaction_metrics(self) -> Dict[str, any]:
        """Calculates user interaction patterns from historical events."""
        if not self._events:
            return {"high_ratio": 0.0, "avg_confidence": 0.0, "time_since_last_high_ms": None}

        high_events = [e for e in self._events if e.priority == "HIGH"]
        high_ratio = len(high_events) / len(self._events)
        avg_confidence = sum(e.confidence for e in self._events) / len(self._events)
        
        time_since = None
        if high_events:
            last_high = max(high_events, key=lambda e: e.event_time)
            # Use max event time observed as current time proxy to avoid clock drift issues
            current_time = max(e.event_time for e in self._events)
            time_since = current_time - last_high.event_time

        return {
            "high_ratio": round(high_ratio, 4),
            "avg_confidence": round(avg_confidence, 4),
            "time_since_last_high_ms": time_since if time_since is not None and time_since >= 0 else 0
        }

    def _train_models(self) -> None:
        """Trains ML models based on collected telemetry data."""
        logger.info(f"Retraining models with {len(self._events)} samples.")
        
        df = pd.DataFrame([e.model_dump() for e in self._events])
        df['hour'] = pd.to_datetime(df['event_time'], unit='ms').dt.hour

        self._train_kmeans(df)
        self._train_decision_tree(df)
        self._train_isolation_forest()

        self._last_trained_count = len(self._events)

    def _train_kmeans(self, df: pd.DataFrame) -> None:
        """Trains KMeans for High-Risk Hours."""
        high_df = df[df['priority'] == 'HIGH']
        if len(high_df) >= Config.KMEANS_N_CLUSTERS:
            features = high_df[['hour', 'intensity', 'duration_ms']]
            self._kmeans = KMeans(n_clusters=Config.KMEANS_N_CLUSTERS, n_init='auto', random_state=42)
            self._kmeans.fit(features)
            logger.info("KMeans model retrained.")

    def _train_decision_tree(self, df: pd.DataFrame) -> None:
        """Trains DecisionTreeClassifier for Directional Risk Zones."""
        if len(df) >= Config.MIN_SAMPLES_FOR_TRAINING:
            features = df[['hour', 'intensity', 'duration_ms']].copy()
            # Approximation: real system would store historical baseline per event
            features['exposure_baseline'] = self._exposure_baseline
            labels = df['direction']
            
            if len(labels.unique()) > 1:
                self._dt_classifier = DecisionTreeClassifier(max_depth=5, random_state=42)
                self._dt_classifier.fit(features, labels)
                logger.info("Decision tree classifier retrained.")

    def _train_isolation_forest(self) -> None:
        """Trains IsolationForest for Alert Frequency anomaly detection."""
        daily_features = []
        for metrics in self._daily_metrics.values():
            if metrics["high_count"] > 0:
                daily_features.append([
                    metrics["high_count"],
                    metrics["total_intensity"] / metrics["high_count"],
                    metrics["total_duration"] / metrics["high_count"]
                ])
                
        # Train if we have enough distinct daily aggregates
        if len(daily_features) >= max(5, Config.MIN_SAMPLES_FOR_TRAINING // 10):
            self._isolation_forest = IsolationForest(
                contamination=Config.ISOLATION_FOREST_CONTAMINATION,
                random_state=42
            )
            self._isolation_forest.fit(daily_features)
            logger.info("Isolation forest retrained.")
