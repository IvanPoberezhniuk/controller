---
name: ugv-operator-control
description: TRIGGER when discussing the UGV's operator input devices (Xbox controller, ExpressLRS), control-authority hierarchy, or the Windows Qt/C++ PC control-station application architecture.
---

# UGV operator control

## Control devices and paths

Previously discussed: Xbox controller, PC control application, ExpressLRS
ES24TX transmitter module, ExpressLRS receiver on the rover, Wi-Fi for
high-bandwidth communication, ELRS for robust low-bandwidth control and basic
telemetry.

```text
Xbox controller
    -> Windows control station
    -> control protocol
    -> ELRS transmitter or Wi-Fi
    -> rover receiver/Pi
    -> STM32 nodes
```

ELRS is appropriate for: command channels, mode selection, emergency state,
basic telemetry (battery voltage, current, RSSI, link quality, GPS, speed,
heading, temperatures, status/fault flags). **ELRS is not suitable for HD
video.**

Wi-Fi is appropriate for: video, SSH, configuration, detailed telemetry,
logs, software updates, future ROS 2 communication.

## Control authority

The command hierarchy must define which source owns control, e.g.:

```text
disabled
manual ELRS
manual PC/Wi-Fi
test mode
future autonomous mode
```

**Only one source may have motion authority at a time.**

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
- SerialWorker or network/CAN gateway client

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
battery data, controller/motor temperatures, current data, CAN/command
status, active drive mode, faults; provide emergency stop; record operator
commands and received telemetry.

**Do not make the PC application the sole location of safety logic** — that
must remain on STM32 regardless of PC state.
