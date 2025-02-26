# ArduinoTaikoController

Sketch for Arduino Leonardo/Micro based controller circuit for taiko games on PC and Nintendo Switch

## Fork alterations

This fork is intended to adapt for a Pro Micro clone board to be the "brains" of the build. This introduces certain limitations, especially regarding the number of accessible pins; we have a total of 14 digital and 4 analog/digital inputs to the device. All four analog pins will need to be used for the piezos; a controller pad will take up eight pins, four are currently dedicated to LEDs, leaving a total of two free inputs potentially used to change the controller mode between Switch controller and keyboard emulation. I also want to update the software to be able to select the mode at runtime based on this pin configuration.

One digital pin can be potentially freed with clever multiplexing of the LEDs, using alternating polarity to control two LEDs off of one pin each, plus one pin to serve as an alternating source/sink. The concept is that used in the FE1.1S chip LED control lines. However, to do so requires editing of the current led_pins functionality. Current thoughts are to store the pin mapping as negatives where driven by the shared pin. This is not intended to be handled immediately; if the need arises, it is an option.

## Software Setup

### Windows 10

If you are using windows 7, you are not able to upload the code for the second time, because of some driver issues. You have to edit the .ini file and add the details of this device in the Arduino driver file to fix this problem. If you have to use Nintendo Switch functions, please do the following steps on a Windows 10 computer.

### Arduino IDE

Install the latest version of Arduino IDE from the official website: [https://www.arduino.cc/en/Main/Software](https://www.arduino.cc/en/Main/Software)

### Modifications to Arduino IDE

TODO: verify these instructions and files in regards to Pro Micro compatibility

Before starting Arduino IDE, to enable nintendo switch functionality, replace the following files with the ones provided in "setup" folder:

- `<your arduino installation path>/hardware/arduino/avr/libraries/HID/src/HID.h`
- `<your arduino installation path>/hardware/arduino/avr/libraries/HID/src/HID.cpp`

Then copy the text in board.txt in "setup" folder and append it to the following file:

- `<your arduino installation path>/hardware/arduino/avr/boards.txt`

If you've successfully done all the modifications above, you should be able to see the board called "Nintendo Switch Controller" next time you start Arduino IDE:

![](https://i.loli.net/2019/03/17/5c8e542c92603.png)

Please select this board before uploading the code as this is essential for your Arduino Leonardo to be recognized by Nintendo Switch.

### Keyboard or Nintendo Switch Controller

To enable or disable keyboard or Nintendo Switch controller functionality, remove or add two characters "//" before these two lines in taiko_controller.ino:

- To enable Switch controller only
```
//#define ENABLE_KEYBOARD
#define ENABLE_NS_JOYSTICK
```
- To enable keyboard only (PC only, doesn't work on Switch)
```
#define ENABLE_KEYBOARD
//#define ENABLE_NS_JOYSTICK
```
- To enable both (not tested)
```
#define ENABLE_KEYBOARD
#define ENABLE_NS_JOYSTICK
```

## Circuit Setup

### Materials

To set up the circuit, you need an Arduino Leonardo (or Pro Micro clone), a set of four piezo sensors, and four 1MΩ resistors for some special cases, and an extra two resistors of the same value for a Pro Micro.

### Connect the Circuit

Connect the sensors to the 3.3v pin and the analog pins according to the diagram below:

![](https://i.loli.net/2019/03/07/5c812d28e0978.png)

The mapping of the sensors by default should be:

- Left Rim: A0
- Left Surface: A3
- Right Surface: A1
- Right Rim: A2

To customize the mapping, checkout the [parameter](#parameters-with-suggested-values) section.

If you're using a Pro Micro, you don't have a 3.3v port to use. Simply connect the two extra resistors as a divider, and use the centre point to give you your source voltage. Alternately, use a regulator.

For most of the times, plugging the sensors directly into Arduino's pins will work.
If the controller seems to be generating random inputs, you can fix this by plugging some 1MΩ resistors in parallel:

![](https://i.loli.net/2019/03/07/5c812d28e101d.png)

### Notes

For best performance, the sensors must be piezo sensors (a.k.a. piezo speakers, contact microphones). No guarantee if other types of sensors will simply work, but if analog signals with voltage ranged 0-5V are fed into analog pins, this setup should be good to go.

For further improvements, you can use some diodes to limit the voltage of the piezo sensors, or use a 2.5v power supply, but this won't matter in most cases, at least on my side.

If you can somehow connect a 4x4 matrix keyboard (no pull-up resistors needed) to Arduino's digital pin 0-7, it will work as a controller along with the drum:

![](https://i.loli.net/2019/03/07/5c813dc59e6a0.png)

## Algorithm

This sketch uses a dynamic threshold of sensor levels to trigger inputs. Whenever the sensor level from one sensor is higher than the threshold, a keyboard or Nintendo Switch controller input is generated, then the sensors will be put into a cooldown period. When an input is triggered or during cooldown period, the threshold will be raised to a ratio of current sensor levels, and after that the threshold will gradually decay by ratio, to hopefully be an envelope of the waves of sensor levels.

As the sensors should have biased input voltages, the sensor levels are the differential value of the analog value from ```analogRead```.

To deal with four analog inputs, we read the sensor levels one at a time and only do the triggering mechanisms for this sensor. To compensate for the time difference, the sensor level for the current one will be a mix of values from previous read and current read. Also, a customized non-blocking version of ```analogRead``` is used to guarantee more stabilization time after a channel switch of Arduino's internal ADC chip.

To deal with Nintendo Switch, I used the HID descriptor for Hori's Pokken fightstick to let Switch trust Arduino as a valid controller device (see the [credits](#credits) section). The default buttons from the four sensors are the analog stick buttons (press the sticks down) and the trigger buttons (ZL and ZR).

As VID and PID of the controller have to be the specific value, the setup to boards.txt is essential. Also, Switch seems to be judging the device strictly by the first-come HID descriptor of the device, so Arduino's default HID behavior have to be altered to have our customized HID descriptor to work.

## Parameters (with suggested values)

#### min_threshold = 15
The minimum value for sensor levels to trigger inputs for all sensors.

To determine an optimal value for this level, try enabling debug info.
Usually, this value is only used to ignore sensor noises, but you can use this level as a sensitivity level.

#### cd_length = 10000
The cooldown length of sensors, in microseconds (=1x10^-6s).

While a sensor is in its cooldown period, no input will be triggered ignoring the sensor level. The threshold level would still be updated if the sensor levels go high.
During the cooldown period, the corresponding key of the sensor is kept pressed. When it ended, the key is released.

#### k_threshold = 1.5
How much the threshold value is raised to, in ratio to the sensor level.

#### k_decay = 0.97
How fast every threshold level decays, in ratio per refresh (about 300ms).

For every refresh, the threshold value is multiplied by k_decay.

#### pin[4] = {A0, A3, A1, A2}
The analog input pins which each of the four piezo sensors are connected to.

#### key[4] = {'d', 'f', 'j', 'k'}
The output mapping of the relative sensors, if keyboard inputs are enabled.

#### sens[4] = {1.0, 1.0, 1.0, 1.0};
Sensitivity of every sensor. All sensor levels are scaled by these values respectively before use.

## Debug Info

If the line ```#define DEBUG_OUTPUT``` is enabled, there will be debug information printed via serial. Take a look at your serial monitor.

The first 4 columns indicate the current vibration level of the four sensors, and the last column indicates the threshold level for a sensor to trigger an input;
the symbols in the middle show the current status of the sensors, # for input triggered and * for cooldown state.

A typical output could be:

```
0  3  13 63 |         | 0
51 2  11 58 | * * * # | 53
83 5  9  24 | # * * # | 83
```

Note that debug output and the matrix keypad MAY be mutually exclusive, depending on the pin usage of the matrix, and if serial is being transcieved on the hardware port or over USB. This is to be confirmed as I experiment with the current setup of the library.

## Credits

- This sketch makes use of Arduino IDE and its useful library. A modified version of the Arduino library is used to allow us to make a valid Nintendo Switch controller.
- The HID descriptor is from progmem's work of reverse engineering of Pokken Tournament Pro Pad: [progmem/Switch-Fightstick](https://github.com/progmem/Switch-Fightstick)
- The HID descriptor is coded using many useful macros and functions from [LUFA Library](http://www.fourwalledcubicle.com/LUFA.php)
