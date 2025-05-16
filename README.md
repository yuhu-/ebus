# C++ library for eBUS communication

This library enables communication with systems based on the eBUS protocol. eBUS is primarily used in the heating industry.

### eBUS Overview

- The eBUS works on a two-wire bus with a speed of 2400 baud.
- Realisation with Standard UART with 8 bits + start bit + stop bit. 
- A maximum of 25 master and 228 slave participants are possible.
- The eBUS protocol is byte-oriented with byte-oriented arbitration.
- Data protection through 8-bit CRC.

### Class Overview

On the bus, a message is sent or received as a sequence of characters.

- The purpose of the **Sequence** (low-level) class is to replace or insert special characters and calculate the CRC byte. 
- The **Telegram** (high-level) class can analyze, generate and evaluate a sequence according to the eBUS specification.
- The **EbusHandler** has routines for sending and receiving all types of telegrams and the option of collecting
statistical data about the eBUS system. To perform this task, the EbusHandler requires a serial bus device.
- **Datatypes** offers functions for decoding/encoding of in accordance with the eBUS data types.

### Tools

**ebusread** interprets all incoming values ​​as eBUS data. The data is validated and then printed to standard
output. Reading from files, devices, pipes and TCP sockets is supported. 

**playground** can be used by developers for testing and experimenting with the eBUS classes.  

### Build

Compilation requires CMake and a C++ compiler (tested on GCC) with C++14 support. 

### [Deprecated]

 **EbusStack** offers the possibility to act as a fully-fledged eBUS participant. It has been implemented
as a PIMPL idiom and runs in its own thread. EbusStack also requires [ppoll](https://man7.org/linux/man-pages/man2/ppoll.2.html) and [pthread](https://man7.org/linux/man-pages/man7/pthreads.7.html) support.


For reporting bugs and requesting features, please use the GitHub [Issues](https://github.com/yuhu-/ebus/issues) page.
