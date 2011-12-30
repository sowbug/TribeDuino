Introduction
============

TribeDuino is a proof of concept that uses an Arduino to read a [Korg Monotribe](http://www.korg.com/monotribe)
audio-encoded firmware file. It doesn't do anything with the data except calculate a checksum to prove that it
read it correctly. The program works by connecting an [Arduino](http://www.arduino.cc/) to an MP3 player of
some kind via an audio patch cable, and then playing the firmware audio file so that the Arduino can hear it.

Background
==========

The firmware uses [frequency-shift keying](http://en.wikipedia.org/wiki/Frequency-shift_keying), or FSK, to
encode its data. In FSK, you pick a point on a sine wave (like the peak), and measure how much time elapses
between peaks. Over time, variations in the peak-to-peak period represent data. The Korg firmware uses binary
FSK, which means that there are only two possible period lengths. A short period is a binary one, and a long
period is a binary zero.

There is a 342-short-bit gap between packets that would seem pointless for a PC that's decoding the WAV file
offline. But for an Arduino or Monotribe that is reading the audio stream real-time, it's a much-needed
75-millisecond break during which the device can do something interesting with the packet it just decoded, such
as writing it to flash memory, without falling behind for the next packet.

The effective bitrate is very close to 2400 baud (calculated using a 33,024-byte final firmware file, 8 bits
per byte, and an audio runtime of about 1:50). That figure is not entirely comparable to a real 2400-baud modem
for various reasons.

Instructions
============

1. Upload the sketch to the Arduino.

1. Get an audio jack. I used one that I probably got from [Sparkfun](http://www.sparkfun.com/products/8032).

1. Connect the back part of the barrel to GND, then the tip to A0.

1. Plug your audio source (computer, iPod, whatever) to the jack. I got good checksums on an iPhone and a laptop.

1. Crank the volume all the way up.

1. Open up the serial monitor @57600.

1. Start playing MONOTRIBE_SYS_0201.m4a when you see "Ready for sound file..." in the serial monitor window.

After about 2 minutes you should see a success message, and you can compare the generated checksum to the one
created by the Python file included with this distribution. We don't print status messages during decoding
because Serial.print() is way too slow to use during that time-critical phase.

Troubleshooting
===============

- Turn the volume all the way up. Don't worry, it won't hurt the Arduino.

- Turn off %&$@! webpages that play ads with sounds. I wasted about an hour before I figured out this was happening.

- If you're going the iPhone route, make sure iTunes didn't re-encode the m4a to 128Kbps AAC, which almost works but doesn't.

- Try again. The sketch is not too solid.

Credits
=======

By [Mike Tsao](http://github.com/sowbug).

This project wouldn't have been possible without the hard work of these folks:

- [nitro2k01](http://blog.gg8.se/wordpress/2011/12/04/korg-monotribe-firmware-20-analysis/)

- [Th0mas/Gravitronic](http://gravitronic.blogspot.com/2011/12/decoding-korg-monotribe-firmware.html)

- [Ludovic Lacoste (aka Ludo6431)](http://ludolacoste.com)

- [arms22](http://code.google.com/p/arms22/). I actually didn't look at this code at all, but it served as a compelling existence proof of an Arduino soft modem.

Korg, if you're reading this, feel free to send me a Monotribe. Thanks.

So What Exactly Is The Concept That This Proves?
================================================

I got interested in this project because I want to build an Arduino development environment that works with minimal
hardware. It'd be cool to be able to develop a sketch on a smartphone web browser or non-jailbroken tablet, then
program the Arduino using just the headphone jack. The Arduino would still need a power source, and to debug with
the IDE the connection would have to be bidirectional. But this is a start.