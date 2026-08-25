---
name: ugv-operator-control
description: TRIGGER when discussing the UGV's operator input devices (Xbox controller, ExpressLRS), control-authority hierarchy, or the Windows Qt/C++ PC control-station application architecture.
---

# UGV operator control

## Control devices and paths

Confirmed radio path: RadioMaster Nomad Gemini Xrossband transmitter module
and RadioMaster XR4 receiver. XR4 connects directly to ESP32 over full-duplex
CRSF. Wi-Fi remains the high-bandwidth link.

```text
XR4 -> ESP32 MANUAL input
Raspberry Pi -> Wi-Fi/IP -> ESP32 AUTO request
ESP32 -> final CAN command -> STM32 nodes
```

ELRS is appropriate for: command channels, mode selection, emergency state,
basic telemetry (battery voltage, current, RSSI, link quality, GPS, speed,
heading, temperatures, status/fault flags). **ELRS is not suitable for HD
video.**

Wi-Fi is appropriate for: Pi video, SSH, configuration, detailed telemetry,
logs, Pi-to-ESP32 future autonomy requests, and future ROS 2 communication.
Raspberry Pi is not a CAN node.

## Control authority

The confirmed command hierarchy is:

```text
disabled
manual ELRS
autonomous Raspberry Pi
```

**Only one source may have motion authority at a time.**
The operator selects the mode explicitly; source failure stops the vehicle and
does not automatically switch to the other source.

## PC control-station software

Existing direction: Windows application, C++17, Qt 6 Widgets, Xmake build
system, modules for control/telemetry/safety/connection/input/UI/logs,
keyboard and XInput support, serial/network workers, planned RTSP video
display.

```text
core/
- App
- Control
- Telemetry
- Safety
- Connection
- LogBuffer

io/
- SerialWorker or Wi-Fi/network gateway client

input/
- Keyboard
- XInput

ui/
- MainWindow
- SettingsDialog
- ConnectionPanel
- ControlPanel
- CompassBar
- ToggleSwitch
```

The PC application should show video, connection state, wheel speeds,
battery data, available controller temperatures, current data, CAN/command
status, active drive mode, faults; provide emergency stop; record operator
commands and received telemetry. Motor temperatures remain unavailable until
external sensors are added in a future revision.

**Do not make the PC application the sole location of safety logic** — that
must remain on STM32 regardless of PC state.
