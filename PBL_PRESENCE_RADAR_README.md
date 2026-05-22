# PBL-Radar

Standalone T-HMI firmware that uses the ESP32-S3 Wi-Fi and BLE radios to infer presence and movement in this room from RF changes.

Current build: `PBL-Radar`, using an adaptive RF baseline. It does not hardcode how many humans are present.

## What It Detects

This is not camera/mmWave human detection. It infers presence from radio patterns:

- Wi-Fi access point count changes.
- BLE advertisement count changes.
- Strong nearby signal changes.
- New or vanished BSSID fingerprints.
- New or vanished BLE device fingerprints.
- RSSI volatility versus a rolling baseline.
- Strong-device density.
- Direction-like RF disturbance sectors.
- Possible movement zones inferred from repeated signal changes in this room.

It cannot identify people, see bodies, or know exact distance. The "human" shapes on screen are privacy-preserving movement hypotheses, not real images.

## RF Inference Engine

The firmware keeps a short rolling RF memory and learns the room dynamically:

- BSSID hashes for nearby radios.
- BLE address hashes for nearby advertisers.
- Previous and current RSSI levels.
- New/vanished radio counts.
- Eight synthetic radar sectors built from Wi-Fi channel and BSSID hash.
- Sector energy from signal deltas and strong nearby radios.
- Exponential moving averages for normal AP count, strongest signal, RSSI average, strong-radio count, and sector energy.
- A dynamic noise floor so normal Wi-Fi drift is less likely to trigger false movement.
- Motion streak and quiet streak hysteresis so one noisy scan does not dominate the display.
- Multi-mover persistence checks so one person causing multipath is less likely to show as two people.

It then combines Wi-Fi volatility, BLE volatility, device flux, baseline drift, sector anomaly, local signal evidence, sensitivity, and motion persistence to produce:

- `R`: this-room confidence.
- `M`: motion/change confidence.
- `X`: software mesh strength from fused Wi-Fi/BLE field disturbance.
- `H?`: inferred mover estimate.
- `S`: RF/BLE silhouette strength.
- `B`: BLE advertiser count.
- `N`: new device count.
- `1 local motion zone`, `2 inferred movers`, or `3+ inferred movers` style tactical hints.

## Screen

- Radar blips represent detected Wi-Fi radios.
- Red/orange stick figures represent likely movement sectors.
- `R` is this-room confidence.
- `M` is motion/change confidence.
- `X` is the software signal-mesh strength.
- `H?` is an inferred mover estimate, not a confirmed headcount.
- `S` is the strength of the live inferred RF/BLE silhouette.
- `B` is nearby BLE advertiser count; magenta squares on the radar are BLE hints.
- Orange/red blobs are temporal RF/BLE shadows built from sector heat and motion trails, not camera imagery.
- The current display uses a restrained pro HUD style: RF contour, sector vectors, sparse blips, metric bars, and status panel.
- The radar view includes a pro heat-map layer: cooler signal zones are blue/cyan, medium energy is yellow/orange, and high inferred activity becomes red.
- Gadget heat appears as compact Wi-Fi/BLE hotspots; movement heat appears as larger fused contour regions.
- The current build includes a software signal-mesh layer. It treats the strongest stable Wi-Fi source, such as a dedicated DIR-1950 hotspot/router, as an RF field anchor and fuses nearby/opposite zone changes into a stronger movement blob.
- The live-scan renderer animates RF field decay, sweep pulses, scan progress, and predicted movement between real Wi-Fi/BLE scans. This makes the current hardware feel continuous while still being honest that the underlying radio samples arrive in scan intervals.
- The top banner shows the main verdict: `DETECTED: HUMAN`, `SIGNAL: POSSIBLE`, or `SCANNING`, plus a confidence percentage.
- The `HUMAN` tab is the end-user view. It shows a spatial room-style map with the T-HMI position, router/RF anchor, field lines, inferred RF silhouette/blobs, human present/absent, duration, stable/movement status, and confidence.
- `THIS ROOM` means the signal change looks local to the T-HMI.
- `NEARBY` means the change could be a wall, adjacent room, hallway, or Wi-Fi environment shift.
- Inference labels include:
  - `THIS ROOM: quiet`
  - `THIS ROOM: occupied`
  - `THIS ROOM: movement`
  - `THIS ROOM: new radios`
  - `NEARBY: RF shift`
  - `Sparse RF room`

## Controls

- `SENS`: cycle sensitivity 1-4.
- `BASE`: set the current room as the quiet baseline.
- `SCAN`: force a Wi-Fi scan.
- `VIEW`: cycle Radar -> Human -> Analytics.
- Top tabs: tap `RADAR`, `HUMAN`, or `ANALY` to switch views directly. The touch area is intentionally larger than the visible tab.
- The bottom-right view button changes label by screen: `HUMAN` on Radar, `ANALY` on Human, and `RADAR` on Analytics.
- Touch controls use large forgiving hit zones at the bottom, with a top-edge fallback for touch calibration quirks.
- GPIO0 short press: move the highlighted bottom button.
- GPIO0 long press: enter/activate the highlighted button.

## Email Alerts

The firmware emits USB serial events when the verdict changes:

- `PBL_RADAR_EVENT,HUMAN_PRESENT,...`
- `PBL_RADAR_EVENT,HUMAN_LEFT,duration=...`

Presence scanning is tuned for faster enter/leave detection: the firmware scans about every 1.35 seconds and uses separate enter/exit hysteresis so a person leaving is not hidden by lingering heat trails.

Run the Mac bridge to email `alerts.com`:

```bash
python3 pbl_radar_email_bridge.py --port /dev/tty.usbmodem1101 --to alerts.com
```

The bridge uses SMTP if `SMTP_HOST`, `SMTP_PORT`, `SMTP_USER`, and `SMTP_PASS` are set. Otherwise it tries the configured macOS Mail app.

## Notes

Better true human presence would need BLE identity tracking, mmWave, PIR, or a paired phone/watch signal. This build stays privacy-friendly: no camera, no microphone.

Room-by-room awareness will need multiple ESP32 nodes later. With one T-HMI, `PBL-Radar` is optimized for this room and can only mark other-room effects as `NEARBY`.

For the current T-HMI + router setup, the router is used as an RF illuminator. It improves signal-field consistency, but the displayed blobs and shape labels are inferred RF shadows, not camera images or medically validated body tracking.
