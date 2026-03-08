import sys
import os
import random
import time
from datetime import datetime, timedelta
import numpy as np
import pandas as pd
from sklearn.metrics import silhouette_score, accuracy_score

from ml_engine import MLEngine
from models import TelemetryEvent

def generate_synthetic_data(num_samples=200):
    events = []
    base_time = int(time.time() * 1000) - (num_samples * 3600 * 1000) # Past events
    
    for i in range(num_samples):
        # Simulate some patterns
        dt = datetime.fromtimestamp(base_time / 1000) + timedelta(hours=i)
        hour = dt.hour
        
        # High risk mostly at night (22-04)
        is_night = hour >= 22 or hour <= 4
        
        if is_night and random.random() > 0.3:
            priority = "HIGH"
            intensity = random.uniform(0.7, 1.0)
            duration = random.randint(1000, 5000)
            direction = random.choice(["BACKWARD", "LEFT", "RIGHT"])
        else:
            priority = random.choice(["LOW", "MEDIUM", "HIGH"])
            intensity = random.uniform(0.1, 0.6) if priority != "HIGH" else random.uniform(0.6, 1.0)
            duration = random.randint(100, 1000) if priority != "HIGH" else random.randint(500, 2000)
            direction = random.choice(["FORWARD", "LEFT", "RIGHT", "BACKWARD"])
            
        event = TelemetryEvent(
            device_id="TEST_DEVICE_1",
            event_id=1000 + i,
            event_time=base_time + (i * 3600 * 1000), # 1 event per hour
            intensity=intensity,
            duration_ms=duration,
            direction=direction,
            priority=priority,
            confidence=random.uniform(0.5, 1.0)
        )
        events.append(event)
    return events

def main():
    print("Initializing MLEngine...")
    engine = MLEngine()
    
    print("Generating synthetic data...")
    events = generate_synthetic_data(200)
    
    print("Processing events to trigger training...")
    for event in events:
        engine.process_event(event) # This will trigger training at 50, 100, 150, 200
        
    print("\n--- Model Evaluation Metrics ---")
    
    df = pd.DataFrame([e.model_dump() for e in engine._events])
    df['hour'] = pd.to_datetime(df['event_time'], unit='ms').dt.hour
    
    #  KMeans Metrics
    if engine._kmeans is not None:
        high_df = df[df['priority'] == 'HIGH']
        features = high_df[['hour', 'intensity', 'duration_ms']]
        labels = engine._kmeans.predict(features)
        if len(set(labels)) > 1:
            score = silhouette_score(features, labels)
            print(f"KMeans (High-Risk Hours Clustering):")
            print(f"  Silhouette Score = {score:.4f} (range -1 to 1, higher is better)")
            print(f"  Total Clusters = {len(set(labels))}")
        else:
            print("KMeans: Not enough clusters formed.")
    else:
        print("KMeans: Model not trained.")
        
    print()
    # Decision Tree Metrics
    if engine._dt_classifier is not None:
        features = df[['hour', 'intensity', 'duration_ms']].copy()
        
        features['exposure_baseline'] = engine._exposure_baseline
        labels = df['direction']
        
        preds = engine._dt_classifier.predict(features)
        acc = accuracy_score(labels, preds)
        print(f"Decision Tree (Directional Risk Prediction):")
        print(f"  Training Accuracy = {acc:.4f} ({acc*100:.1f}%)")
        print(f"  Classes predicted = {list(set(preds))}")
    else:
        print("Decision Tree: Model not trained.")
        
    print()
    #  Isolation Forest Metrics
    if engine._isolation_forest is not None:
        daily_features = []
        for metrics in engine._daily_metrics.values():
            if metrics["high_count"] > 0:
                daily_features.append([
                    metrics["high_count"],
                    metrics["total_intensity"] / metrics["high_count"],
                    metrics["total_duration"] / metrics["high_count"]
                ])
        if len(daily_features) > 0:
            preds = engine._isolation_forest.predict(daily_features)
            anomalies = list(preds).count(-1)
            normal = list(preds).count(1)
            print(f"Isolation Forest (Alert Frequency Anomaly Detection):")
            print(f"  Detected {anomalies} anomalous days and {normal} normal days")
            print(f"  Contamination Ratio = {anomalies / len(preds):.2f}")
    else:
        print("Isolation Forest: Model not trained.")

if __name__ == '__main__':
    main()
