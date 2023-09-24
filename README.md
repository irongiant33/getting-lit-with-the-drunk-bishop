# Getting Lit with the Drunk Bishop

## Installation

Install the latest version of the Arduino IDE. In the "Tools" tab, go to the "Manage Libraries" option and select the following:
- HX711 By Rob Tillaart (https://github.com/RobTillaart/HX711): I ran this on version 0.3.8
-  XxHash_arduino by atesin (https://gitlab.com/atesin/XxHash_arduino): I ran this on version 2.1.0

Verify your code with the check mark icon in the top left of the IDE, then upload the `bar_led.ino` sketch to the Arduino Uno with the upload icon. 

Connect all wirings according to the KiCad schematic diagram.

## Usage

The code requires no user interaction other than placing a glass or other weighted object on the scale.

If you wish to view the plots that I showed in the blog, run the `drunk_bishop.ino` on your Arduino Uno and select the "Tools" tab then "Serial Plotter". 

## Future Implementations

- [ ] KiCad schematic for PCB
- [ ] STL for 3D printed coaster