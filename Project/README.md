# Remote-Controlled Auto-Tracking Camera: Yaw Subsystem

ENSC 351 project. This folder holds the yaw (pan) control code for the BeagleY-AI.

## Layout

- `src/`: C source for the ADC, servo, encoder and yaw modules, plus `main`
- `tests/`: small test programs (ADC readout, servo sweep)
- `docs/`: wiring diagram and calibration notes
- `build/`: build output (don't commit)

## Hardware (yaw only)

- BeagleY-AI
- MCP3208 ADC over SPI0 (CE0). Joystick VRx goes to CH0. Joystick VCC is 3.3 V.
- SG90 yaw servo. Signal on GPIO12 (pin 32). Powered from its own 5 V supply, with grounds shared.
- Rotary encoder: CLK and DT for nudge/speed, SW for the software lock.
- Potentiometer: not used for now.

## Status

- [ ] SPI enabled and ADC reads working
- [ ] Joystick calibrated (centre, deadzone)
- [ ] Yaw servo follows joystick
- [ ] Encoder software lock
