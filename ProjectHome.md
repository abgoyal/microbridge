<font color='red'><b>Note: Microbridge is provided as-is. I do not have the time to provide personal support. Please do not contact me with questions unless you are absolutely certain that a) you know what you are doing and b) your problem is related to Microbridge directly. Thank you for your understanding.</b></font>

# What's this? #

MicroBridge is an [Android Debug Bridge (ADB)](http://developer.android.com/guide/developing/tools/adb.html) implementation for microcontrollers. MicroBridge allows stock, unrooted Android devices to talk directly to USB host enabled MCUs, thereby enabling phones to actuate servos, drive DC motors, talk to I2C and SPI devices, read ADCs, and so forth. MicroBridge works on Android 1.5 and upwards.

<a href='http://www.youtube.com/watch?feature=player_embedded&v=QW41rj0OQT0' target='_blank'><img src='http://img.youtube.com/vi/QW41rj0OQT0/0.jpg' width='425' height=344 /></a>

# How does it work? #

The ADB protocol supports opening a shell on the phone or issuing shell commands directly, pushing files to the phone, reading debugging logs ([logcat](http://developer.android.com/guide/developing/tools/logcat.html)), and forwarding TCP ports and Unix sockets. Using TCP sockets it's possible to establish bidirectional pipes between an Arduino and an Android device. The Android application listens on a port, and the Arduino connects to that port over ADB.

When the connection is lost because the application crashed or because the USB device was unplugged, MicroBridge periodically tries to reconnect. When the device is plugged back in, the link is automatically reestablished.

# Software #

MicroBridge is fully functional and able to do pretty much anything you can do with the command line ADB tool. Two versions exist, one written in C, and a C++/Wiring port for Arduino.

The Arduino library can be used with the Arduino IDE, and includes a couple of examples, including forwarding a unix shell, rebooting your phone, logcat output, and a small demo application showcasing bidirection communication between Arduino and Android.

# Hardware #

The current supported hardware is Arduino ([Uno](http://arduino.cc/en/Main/ArduinoBoardUno), [Deicimila](http://www.arduino.cc/en/Main/ArduinoBoardDiecimila), [Mega](http://arduino.cc/en/Main/ArduinoBoardMega), or [compatible](http://www.arduino.cc/playground/Main/SimilarBoards)) and Oleg Mazurov's [USB host shield](http://www.circuitsathome.com/products-page/arduino-shields/usb-host-shield-2-0-for-arduino/). Sparkfun carries a [slightly cheaper version](http://www.sparkfun.com/products/9947) of the same shield, although this requires VIN on the Arduino.

Since Google's ADK was announced, other stores have started selling (Arduino Mega compatible) USB host shields for Arduino, such as [Emartee](http://emartee.com/product/42089/Arduino%20ADK%20Shield%20For%20Android), and [DFRobot](http://www.dfrobot.com/index.php?route=product/product&filter_name=usb%20host&product_id=498). Single-board solutions are also available, such as this [Arduino Mega ADK](http://www.dfrobot.com/index.php?route=product/product&filter_name=usb%20host&product_id=501) from DFRobot or [Arduino directly](http://store.arduino.cc/eu/index.php?main_page=product_info&cPath=11_12&products_id=144). Seeed studios sells their [Seeeduino ADK](http://www.seeedstudio.com/depot/seeeduino-adk-main-board-p-846.html?cPath=132_133).

Finally, Oleg also sells a [smaller max3421e breakout board](http://www.circuitsathome.com/products-page/breakout-boards/usb-host-shield-for-arduino-pro-mini/) for use with the [Arduino Pro Mini](http://www.arduino.cc/en/Main/ArduinoBoardProMini).

# Compatibility #

MicroBridge has been confirmed to work with several phones, including the ZTE Blade, Nexus S, Samsung Galaxy S, and HTC Desire HD. We have not yet encountered a device that it did not work with, so it is likely that it will work with yours as well.

I'm interested to hear from you if you've used MicroBridge to talk to a device that's not on the list.

# Alternatives #

Since the initial release of MicroBridge several alternatives have emerged. First of all, [PropBridge](http://robots-everywhere.com/re_wiki/index.php?title=PropBridge) is based on MicroBridge but instead uses a propeller chip and very little external hardware. A PIC24 port of Microbridge that uses the PIC24FJ64GB002 can be found [here](http://code.google.com/p/microbridge-pic/). Sparkfun is selling a device called [IOIO](http://www.sparkfun.com/products/10585), an open source PIC24-based device which does not require any firmware programming but instead lets the Android device control its IO pins directly. Finally, Google announced [ADK](http://developer.android.com/guide/topics/usb/adk.html), the open accessory development kit. The ADK reference design is based on Arduino and Oleg's USB host shield.