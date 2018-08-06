# CeleX-SDK
SDK version 2.2 for CeleX sensor.

CeleXTM is a family of smart image sensor, specially designed for machine vision. Each pixel in CeleXTM
sensor can individually monitor the relative change in light intensity and report an event if a threshold is
reached.

The output of the sensor is not a frame, but a stream of asynchronous digital events. The speed of the sensor
is not limited by any traditional concept such as exposure time and frame rate. It can detect fast motion
which is traditionally captured by expensive, high speed cameras running at thousands of frames per second,
but with drastic reduced amount of data.

Our technology allows post-capture change of frame-rate for video playback. One can view the video at
10,000 frames per second to see high speed events or at normal rate of 25 frames per second.

This SDK provides an easy-to-use software interface to get data from the sensor and communicate with the
sensor, and it is consistent across the Windows (32-/64-bit) and Linux (32-/64-bit) development
environments. In addition, it provides both pure C++ interfaces without any third libraries and
OpenCV-based interfaces to obtain data from the sensor.

This SDK provides three working modes of CeleXTM Sensors: Full-Picture data, Event data, and
Optical-Flow data. Full-Picture and Event data output alternately to create FullPic-Event data.
