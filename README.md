# CeleX-SDK
SDK for CeleX sensor.

![Structure](https://github.com/CelePixel/CeleX-SDK/raw/master/Sources/CeleXDemo/images/SDK_Structure.png)

* CeleX is a family of smart image sensor, specially designed for machine vision. Each pixel in CeleX
sensor can individually monitor the relative change in light intensity and report an event if a threshold is
reached.

* The output of the sensor is not a frame, but a stream of asynchronous digital events. The speed of the sensor
is not limited by any traditional concept such as exposure time and frame rate. It can detect fast motion
which is traditionally captured by expensive, high speed cameras running at thousands of frames per second,
but with drastically reduced amount of data.

* Our technology allows post-capture change of frame-rate for video playback. One can view the video at
10,000 frames per second to see high speed events or at normal rate of 25 frames per second.

* This SDK provides an easy-to-use software interface to get data from the sensor and communicate with the
sensor, and it is consistent across the Windows (32-/64-bit) and Linux (32-/64-bit) development
environments. In addition, it provides both pure C++ interfaces without any third libraries and
OpenCV-based interfaces to obtain data from the sensor.

`The CeleX-SDK is structured as follows:`

* _DemoGUI_: 
  * _CeleX4_: CeleX-4 Demo GUI execution (Windows and Linux).
  * _CeleX5_: CeleX-5 Demo GUI execution (Windows and Linux).
* _Documentation_:
  * _CeleX4_SDK_Reference_: The introduction of CeleX-4 sensor and the references of all the classes and functions in the SDK.
  * _CeleX4_SDK_Getting_Started_Guide_: The instructions to use the CeleX-4 sensor demo kit, install OpalKelly driver, run the CeleXDemo GUI and compile the source code.
  * _CeleX5_SDK_Reference_: The introduction of CeleX-5 sensor and the references of all the classes and functions in the SDK.
  * _CeleX5_SDK_Getting_Started_Guide_: The instructions to use the CeleX-5 sensor demo kit, install CX3 USB3.0 driver, run the CeleXDemo GUI and compile the source code.
* _Drivers_: 
  * _CeleX4_: OpalKelly FPGA Board driver (Windows / Linux / ARM).
  * _CeleX5_: CX3 USB3.0 driver (Windows / Linux).
* _Sources_:
  * _SDK_: Source code of CeleX library (including both CeleX-4 and CeleX-5).
  * _CeleXDemo_: Source code (developed by Qt) of CeleX demo.
  * _FPGA_: SE Project files, top level source code, Lower Layer FPGA module Declaration and Netlist file (ngc file).
  * _CeleDriver_: Source code of CX3 USB3.0 driver.
* _Samples_: Several examples developed based on SDK and a sample user manual file.
* _ReleaseNotes.txt_: New features, fixed bugs and SDK development environment.
