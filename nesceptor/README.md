NESceptor
=========

The NESceptor is a small hardware mod that adds a USB output to a 1985 Nintendo Entertainment System.
This mod is designed to provide support for speedrunning and speedrunning competitions.
The primary use case is to:

 - Allow for live RAM watch
 - Transmit console power status
 - Transmit precise timing information

Future functionality might provide pixel perfect video capture and with audio!? Investigations into this capability is ongoing..

Design Decisions
----------------

### Why this overall design?

Firstly, the console is to be treated with respect.
Although nearly [62 million units](https://en.wikipedia.org/wiki/Nintendo_Entertainment_System) were sold, they are no longer manufactured and should not be unnecessarily damaged.
The case of the Nintendo Entertainment system is _not_ to be cut or irreversibly modified.
The electronics are _not_ to be irreversibly modified.

To satisfy this requirement - while also balancing the desires to be able to quickly install a NESceptor - a two-part design was chosen.
The first board sits inside the console and attaches directly to the back of the CPU via a series of castellations.
This first board provides level translation and hosts the microcontroller which does all necessary i/o and processing.

The second part sits in the small outside slot on the left hand side of the console and is attached via double sided tape and a small flat cable which goes back to the in console pcb.
This second board has ESD protection and the USB port for power in and data trasmission out.

A rough block diagram is below:

```
                     +--inconsole/--------------------+   +--usbport/---------+
     +------------+  |  +--------+    +------------+  |   |                   |
     |            |  |  |        |    |            |  |   | USB 1.1 Type B    |
     | NES CPU    |  |  | 5V to  |    | Rp2040     |  |   |                   |
     |            |  |  | 3.3V   |    | Micro-     |  |   | ESD protection?   |
     | 8 data     |  |  |        |    | controller |  |   |                   |
     | 14 addr    | --> | Level  | -> |            | ---> | Power?            |
     | 4 aux      |  |  | Trans- |    | Watching / | <--- |                   |
     | --         |  |  | lators |    | Filtering  |  |   |                   |
     | 26 signals |  |  |        |    |            |  |   |                   |
     |            |  |  |        |    |            |  |   |                   |
     +------------+  |  +--------+    +------------+  |   |                   |
                     +--------------------------------+   +-------------------+ 
```

### Why Castellations?

The `inconsole` pcb is connected to the NES CPU with 40 castellations.
Consider:

- Removing the CPU and/or PPU can be difficult for novices (ahem) and requires patience, dedicated tools, and practice.
  Once removed a socket is often installed, and the CPU/PPU have to be installed into yet another socket, and the whole thing is fiddly and may lead to damage.
- The CPU itself is not subjected to heat or potential damage, and is left mostly alone with this design.
- A flex pcb was tried at one point, but it proved very difficult to install and remove, and is more expensive to manufacture.

The main drawback to the castellations is that they may be difficult to remove.

### Which signals are needed from the console?

The following signals are absolutely required:

- Data lines 0 to 7 (8 total), these are self explanatory!
- Address lines 0 to 10 and 13-15 (14 total). Address lines 11 and 12 are not necessary because they are [unmapped](https://www.nesdev.org/wiki/CPU_memory_map).
- `NES_RST` when low the console is off, and that is important to know.
- `M2` is the clock
- `R/W` for read / write (low is write) 

This gives a total of 25 signals.

Historically `NMI` was also watched - but it was not used in the first version.
Maybe the controller pins are important, or `IRQ`, but they were never hooked up.

### Notes on level translation

The NES is a 5V system, which is relatively high by modern standards, as most modern microcontrollers only have 3.3V tolerant inputs.
For example the [RP2040 datasheet](https://www.mouser.com/datasheet/2/635/rp2040_datasheet-3048960.pdf) indicates a maximum 3.63V supply voltage (`IOVDD`) and `IOVDD + 0.3` as maximum input voltage.
So as not to damage the microcontrollers the NES signals must be translated.

Some considerations

- There are 25 channels to translate which is annoyingly _one more_ than 24 (a nice multiple of both 6 and 8), but it is what it is.
- The level translators must act as high impedence when unpowered, so that the console operates when the mod is not plugged in.
- The level translators must not be used to change or interact with the console whatsoever.
  This is imperitive to maintain the legitimacy of the speedrunning, and must be clearly verifiable by third parties.

*TODO @Ryan to help design / choose components and justify that choice :)* 

- Maybe the [TXU0104](https://www.ti.com/product/TXU0104)

### Why the RP2040?

An earlier version of the NESceptor used an FPGA - specifically the Lattice iCE40 HX on the Alchitry CU development board, however:

- FPGAs are expensive.
- FPGAs are difficult to program (requiring some HDL).

The RP2040 is cheaper and clearly capable of reading the NESs roughly 2 MHz signals:

- [This logic analyzer project](https://github.com/gusmanb/logicanalyzer/tree/master) uses a raspberry pi pico and boasts 100Msps
- And [this one](https://github.com/dotcypress/ula) also gets similar speeds.

One concern is on the GPIO pins.
Apparently there are exactly 26 programmable GPIOs which is just enough.

### Why USB

USB is simple and well supported.
Ethernet is not necessary because the nesbox mini pc will handle that.

