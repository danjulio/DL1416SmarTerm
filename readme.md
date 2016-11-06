## DL1416SmarTerm

This repository contains the code for the DL1416SmarTerm retro-computer.

It's designed to run on Teensy 3.1 (3.2) connected to a display made of Siemens
DL1416B intelligent LED modules controlled by firmware running on PIC micro-controllers
and a keyboard with a serial output.  The code is compiled using the Teensy Arduino
environment.

It provides both a VT102-ish compatible terminal and Tiny Basic (based on Scott
Lawrence's version of Mike Field's port) selectable by switch (digital input) with
printer support and a host interface over both a TTL serial port or a USB CDC class
serial interface.  Tiny Basic provides 9 IO ports for user access, each including both
digital IO and analog input.  Three of the IO ports also provide PWM output.

Unless you are building a replica of the DL1416SmarTerm it's unlikely this code will
be useful as-is.  However it's offered in the spirit of open-source.  Maybe it will
help some other project, for example a terminal driving some off-the-shelf intelligent
LCD display and PS2 keyboard or even (I hope, someday) a Teensy with a built-in driver
for NTSC/PAL or VGA output.

###Questions?

Contact the author @ dan@danjuliodesigns.com
