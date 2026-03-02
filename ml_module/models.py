from pydantic import BaseModel, Field
from typing import Literal

class TelemetryEvent(BaseModel):
    device_id: str = Field(..., min_length=1, description="Unique identifier of device")
    event_id: int = Field(..., gt=0, description="Unique event identifier")
    event_time: int = Field(..., gt=0, description="Unix timestamp in ms")
    intensity: float = Field(..., ge=0.0, le=1.0, description="Physical intensity of the event")
    duration_ms: int = Field(..., gt=0, description="Duration in milliseconds")
    direction: Literal["UP", "DOWN", "LEFT", "RIGHT", "FORWARD", "BACKWARD"]
    priority: Literal["LOW", "MEDIUM", "HIGH", "CRITICAL"]
    confidence: float = Field(..., ge=0.0, le=1.0, description="Confidence score")
