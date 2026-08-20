---
name: ugv-camera-streaming
description: TRIGGER when discussing the UGV's camera hardware, capture pipeline, or video streaming — IMX708/Camera Module 3, rpicam-vid, MediaMTX, RTSP endpoint, or the Qt control station's video display.
---

# UGV camera and video streaming

## Confirmed camera

Raspberry Pi Camera Module 3-class IMX708 sensor, connected to Raspberry Pi 5
via 22-pin to 22-pin FFC cable.

Observed modes (camera capability, not necessarily final streaming modes):
approximately 1536x864 up to 120 FPS; 2304x1296 approximately 56 FPS;
4608x2592 approximately 14.35 FPS.

## Current streaming stack

Confirmed components: MediaMTX (previously around version 1.18.2),
systemd-managed service, RTSP endpoint `rtsp://roverpi.local:8554/ugv`,
capture pipeline using `rpicam-vid`, FFmpeg forwarding the H.264 stream to
MediaMTX.

Representative configured stream: 1280x720, 30 FPS, H.264, intra interval
approximately 15 frames.

```text
rpicam-vid
    -> H.264 stdout
    -> FFmpeg
    -> MediaMTX
    -> RTSP client
```

**Detect the installed capture command rather than assuming one name.**

## Video usage

Initial control station: Windows PC, Qt 6 application, video consumed via
RTSP or another supported low-latency method. RTSP is the current implemented
path.

Future alternatives: WebRTC for browser access and lower-latency
NAT-friendly communication; GStreamer RTP/UDP for controlled local networks.
**Do not replace the working RTSP path without a measurable benefit.**

**Video loss must not disable the independent command timeout and motor
failsafe** (that lives on STM32 regardless of video/network state).
