# Getting started #

This short guide will help you to get started with MicroBridge.

# Hardware #

MicroBridge was developed using a [Sparkfun USB Host shield](http://www.sparkfun.com/products/9947), in combination with one of the 'smaller' Arduinos like the [Uno](http://arduino.cc/en/Main/ArduinoBoardUno) and [Deicimila](http://www.arduino.cc/en/Main/ArduinoBoardDiecimila).

Here's a picture of a Uno with the USB host shield connected to my Android phone. In case you were wondering, it's a [ZTE Blade](http://en.wikipedia.org/wiki/ZTE_Blade).

![http://i54.tinypic.com/b7j5o8.jpg](http://i54.tinypic.com/b7j5o8.jpg)

Note the external power source to supply VBUS. Alternatively you can also use Oleg Mazurov's [USB host shield 2.0](http://www.circuitsathome.com/products-page/arduino-shields/usb-host-shield-2-0-for-arduino/) which doesn't need an external power source and compatible with the Arduino Mega, which the Sparkfun shield is not.

# Software #

I'm going to assume you have basic programming knowledge and know your way around an Arduino and the Arduino IDE. If you don't want to use the Wiring system, you can always use the [C version](http://code.google.com/p/microbridge/source/browse/#svn%2Ftrunk%2Fsrc%2Fmcu). I'm currently using the Arduino IDE version 0022.

## Running the examples ##

Download the Adb library [here](http://code.google.com/p/microbridge/downloads/detail?name=MicroBridge-Arduino.zip&can=2&q=#makechanges) and unpack it into the libraries directory of your Arduino installation, this should give you an 'Adb' directory.

Restart the IDE if you already had it running to make it detect the new library. If the installation was successful, you should be able to go into file->examples and see an Adb submenu there. Select the Shell example and upload it to your device. On your phone, enable debugging if you hadn't already (settings->applications->development->USB debugging). Now connect your phone to the host shield and open a terminal to the Arduino (57600 baud). If all is working as it should, you should now be presented with an interactive Unix terminal.

## Demo Application ##

Choose the Demo application from the examples and upload it to your Arduino board. Connect two servos to digital pins 2 and 3, and an analog sensor (pot meter, distance sensor, etc.) to A0.

Download the Android application [here](http://code.google.com/p/microbridge/downloads/detail?name=MicroBridge-DemoApp.zip&can=2&q=#makechanges), import into eclipse, compile, and run on your phone. This demo application contains a small TCP server that listens on port 4567, which you can use for your own projects if you wish.

The result should look something like the following:

<a href='http://www.youtube.com/watch?feature=player_embedded&v=nbtDwlXw29I' target='_blank'><img src='http://img.youtube.com/vi/nbtDwlXw29I/0.jpg' width='425' height=344 /></a>