# PBL Radar

Camera-free presence and room-safety sensing prototype.

## Current Prototype

The current T-HMI prototype uses the LilyGO T-HMI ESP32-S3 built-in Wi-Fi and BLE radios to scan nearby signal sources and infer presence/movement from RF changes.

It does not use the router's hardware. The T-HMI itself scans Wi-Fi beacons and BLE advertisements.

## Important Accuracy Note

The T-HMI-only build is an RF inference prototype. For product-grade human presence detection, the plan is to move to a low-cost headless unit:

- ESP32-S3 or ESP32-C3
- HLK-LD2450 or similar 24 GHz mmWave radar
- Optional humidity/temperature, AQI/gas, or vibration sensors

## Files

- `firmware/presence_radar.ino`: T-HMI PBL-Radar firmware.
- `bridge/pbl_radar_email_bridge.py`: serial-to-email alert bridge.
- `bridge/pbl_radar_daily_summary.py`: daily summary helper.
- `PBL_PRESENCE_RADAR_README.md`: prototype notes.
- `PBL_RADAR_PRODUCT_PLAN.md`: product/SaaS strategy.

## Example Serial Status

```text
PBL_RADAR_STATUS,present=1,confidence=92,room=100,motion=59,silhouette=88,ble=6,ap=11
```

In that scan, the T-HMI heard 11 Wi-Fi access-point signals and 6 BLE advertising signals.
