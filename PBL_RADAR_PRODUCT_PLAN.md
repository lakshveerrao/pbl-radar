# PBL Radar Product Plan

Camera-free presence, safety, and operations intelligence for private spaces.

## Core Thesis

PBL Radar should not be sold as a gadget. It should be sold as a privacy-first room intelligence network.

The hardware is the entry point: a small plug-and-play ESP32 + mmWave sensor unit. The real company is the software layer that converts cheap room sensors into useful decisions:

- Is someone present?
- Did someone enter or leave?
- Has someone been in a bathroom too long?
- Is an elder moving normally today?
- Is there abnormal stillness?
- Is air quality unsafe while someone is present?
- Is a machine vibrating unusually?
- Which rooms are active, quiet, risky, or need attention?

The strongest wedge is places where cameras feel wrong or are not allowed: bathrooms, bedrooms, elder rooms, clinics, hostels, hotels, care homes, rental properties, and small facilities.

## Palantir-Style Insight

The Palantir lesson is not dashboards. It is an operational ontology.

For PBL Radar, the ontology should model:

- `Room`: bathroom, bedroom, kitchen, office, machine room.
- `Person State`: present, absent, moving, still, lingered, returned, abnormal pattern.
- `Sensor State`: mmWave target, motion vector, humidity, temperature, AQI, gas, vibration, battery/power, Wi-Fi health.
- `Event`: entered, left, too long, no movement, air unsafe, device offline, tamper/reset.
- `Risk`: fall suspicion, privacy-safe occupancy, ventilation risk, unattended room, abnormal routine.
- `Action`: notify, escalate, summarize, silence, call caregiver, create task, open maintenance ticket.

This becomes the moat. Competitors can copy a sensor box. It is harder to copy a room-state engine that gets better across homes, facilities, room types, and workflows.

## Product Name

Working platform name: `PBL Radar`

Possible product-line names:

- `PBL Radar Bath`: camera-free bathroom safety monitor.
- `PBL Radar Care`: elder room and bedroom presence monitor.
- `PBL Radar Air`: presence-aware air quality and gas safety monitor.
- `PBL Radar Secure`: privacy-safe occupancy and intrusion monitor.
- `PBL Radar Machine`: vibration plus presence safety for equipment areas.
- `PBL Radar Hub`: optional local gateway/dashboard for homes and facilities.

## First MVP

Start with `PBL Radar Bath`.

Reason: the problem is obvious, emotionally strong, privacy-sensitive, and easy to explain.

Promise:

> A camera-free bathroom safety monitor that tells family or staff when someone enters, leaves, stays too long, or stops moving unusually.

What it should do first:

- Detect human entered.
- Detect human left.
- Track duration inside.
- Alert if duration crosses a configurable threshold.
- Alert if presence is detected but movement becomes unusually still.
- Track humidity/temperature so it can separate shower usage from abnormal occupancy.
- Send mobile/web/email alerts.
- Provide daily summaries without showing images, audio, or identity.

Avoid medical claims in early versions. Say safety monitor, wellness monitor, privacy-safe presence monitor. Do not claim fall detection until validated.

## Hardware Strategy

No T-HMI in the product unit. T-HMI remains a dev console, demo unit, or optional installer/debug tool.

Base unit:

- ESP32-S3 or ESP32-C3 depending cost and memory needs.
- HLK-LD2450 or similar 24 GHz mmWave module as the primary human presence sensor.
- USB-C power.
- One reset/pairing button.
- One simple status LED or light pipe.
- Enclosure designed for wall plug, shelf, ceiling, or adhesive mount.
- No camera.
- No microphone.

Sensor variants:

- Bath/Care: mmWave + temperature/humidity.
- Air: mmWave + VOC/AQI/gas.
- Machine: mmWave + vibration.
- Secure: mmWave + door/contact or optional PIR.
- Pro facility unit: mmWave + temp/humidity + AQI + offline buffer + better antenna/enclosure.

## Onboarding

The product must feel appliance-simple:

1. Plug in.
2. Open phone/laptop onboarding page.
3. Connect Wi-Fi.
4. Pick room type.
5. Done.

The device should auto-learn room baseline. No tuning screen for normal users.

One physical button:

- Short press: status/check-in.
- Long press: pairing mode.
- Very long press: factory reset.

## Software Platform

Three layers:

1. Edge firmware:
   - LD2450 target parser.
   - Sensor fusion.
   - Enter/leave state machine.
   - Local baseline learning.
   - Offline event buffer.
   - OTA updates.
   - Privacy-preserving local event generation.

2. Cloud/API:
   - Device registry.
   - Room ontology.
   - Event timeline.
   - Alert engine.
   - Daily summaries.
   - Caregiver/facility roles.
   - Fleet monitoring.
   - OTA rollout.

3. App/dashboard:
   - Live room state.
   - Event history.
   - Alert settings.
   - Occupancy duration.
   - Routine changes.
   - Device health.
   - Facility map for many rooms.

## AI Layer

Use AI carefully. The edge device should not depend on an LLM for basic safety alerts.

Good AI uses:

- Learn normal room patterns.
- Reduce false positives by room type.
- Summarize daily activity.
- Explain why an alert fired.
- Detect routine change over days/weeks.
- Auto-suggest threshold settings.
- Turn raw events into caregiver-friendly notes.

Bad AI uses:

- Pretending to identify people.
- Inventing silhouettes from weak RF data.
- Making medical/fall claims without validation.
- Sending alerts based only on noisy Wi-Fi/BLE scans.

## Business Model

Hardware:

- Low-cost base unit with healthy margin.
- Bundles for bathrooms, elder rooms, small facilities.
- Optional hub for facilities or offline-first customers.

SaaS:

- Free: one room, basic enter/leave and duration.
- Family: multi-room, alerts, daily summary, history.
- Care: caregiver escalation, abnormal routine detection, multiple users.
- Facility: room map, staff workflows, fleet health, reports, API.

Stickiness comes from:

- Historical routines.
- Caregiver network.
- Alert rules.
- Multi-room setup.
- Facility workflows.
- Trust in low false alarms.
- Privacy-first positioning.

## Beachhead Markets

Best first targets:

- Families caring for elders at home.
- Bathroom safety for seniors.
- Small elder care homes.
- Hostels/PGs where cameras are unacceptable.
- Clinics and washrooms.
- Hotels that need occupancy/maintenance signals without cameras.

Avoid starting with generic smart home. It is too broad and too price-sensitive.

## Differentiation

Against cameras:

- Works where cameras cannot.
- No images, no faces, no audio.
- Safer for bedrooms and washrooms.

Against PIR:

- Can detect still presence better.
- Can track duration.
- Better for bathrooms and elder rooms.

Against generic mmWave sensors:

- Plug-and-play onboarding.
- Useful alerts and summaries.
- Room-state intelligence, not raw sensor readings.
- Multi-room SaaS and caregiver/facility workflows.

Against Wi-Fi-only RF sensing:

- More direct human sensing through mmWave.
- Uses Wi-Fi/BLE only as supporting context, not the main truth source.

## Data Moat

The valuable dataset is not raw RF. It is labeled room events:

- Room type.
- Time of day.
- Enter/leave duration.
- Movement/stillness pattern.
- Humidity/air/vibration context.
- Alert outcome: useful, false alarm, ignored, escalated.
- Device placement quality.

This lets PBL Radar learn better defaults by room type and customer segment.

## Investor Story

PBL Radar is building the privacy-safe sensing layer for human spaces.

The first product is a camera-free bathroom and elder safety monitor. The larger vision is an operating system for private-room intelligence: homes, care facilities, hotels, clinics, factories, and public infrastructure where cameras are invasive or impossible.

The company starts with a simple device, but compounds through software: room ontology, sensor fusion, alert workflows, daily summaries, fleet health, and AI-assisted operations.

## 90-Day Plan

Month 1:

- Build ESP32 + LD2450 prototype.
- Add humidity/temperature sensor.
- Create stable enter/leave firmware.
- Build local web dashboard.
- Validate in 2-3 rooms.

Month 2:

- Add cloud event ingestion.
- Add email/WhatsApp/mobile push prototype.
- Build onboarding flow.
- Build daily summaries.
- Test with 5-10 pilot users.

Month 3:

- Design enclosure.
- Build 20-50 pilot units.
- Measure false positives/false negatives.
- Add caregiver/facility dashboard.
- Prepare investor demo with real timelines, alerts, and privacy story.

## Immediate Engineering Next Step

Move from T-HMI Wi-Fi/BLE inference to real product hardware:

- ESP32-S3 dev board.
- HLK-LD2450 mmWave module over UART.
- SHT40/SHT31 temperature/humidity sensor.
- Simple local web UI.
- Event firmware with `ENTERED`, `LEFT`, `TOO_LONG`, `NO_MOTION`, and `DEVICE_OFFLINE`.

T-HMI can remain the flashy prototype screen, but the product must prove itself on the cheap headless sensor unit.
