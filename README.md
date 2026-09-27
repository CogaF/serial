This fork is intended to keep track of my personal needed modifications which consist in:
- method getPort(), made it to return wstring instead of string. This to avoid warning in building the library
- method open(), made it to not throw exception and if port is already open, close and reopen the port. This because, for my purpose, it is not useful to have and exception thrown.
- Visual Studio build of the library as a **static library and a DLL**, Debug and Release, x64 and x86 (see below).

## Building with Visual Studio (Windows)

Run **`build_all.bat`** (finds Visual Studio 2022 or later by itself), or open
**`visual_studio\visual_studio.sln`**, pick *Debug/Release* and *x64/x86* and **Build Solution** - each
build makes both the static library and the DLL, and runs a smoke test against each (no serial
hardware needed; a broken library fails the build).

```
Builds\
  include\serial\serial.h, v8stdint.h
  x64\Debug\lib\serial.lib                       static library (debug info inside)
  x64\Debug\dll\serial.dll, serial.lib, .pdb      DLL, its import library, symbols
  x64\Release\...   x86\Debug\...   x86\Release\...
```

Using it: add `Builds\include` to the include directories and link `serial.lib` from the `lib` or the
`dll` folder of your platform and configuration. **With the DLL, define `SERIAL_USE_DLL`** in your
project and copy `serial.dll` next to your exe. The libraries use the DLL C runtime (`/MD`, `/MDd`,
the Visual Studio default); `build_all.bat static-crt` builds `/MT` versions into `Builds_StaticCRT\`.
No whole-program optimization (`/GL`) is used, so the libraries link with the same or any later
Visual Studio. `visual_studio\test_serial` is the original interactive example (not built by default).

I'm keeping it public in case anyone feels this modification is helpful. Below the original readme of the library.

### Author of the (tiny) modification


Coga Fation <coga.fation@gmail.com>


# Serial Communication Library

[![Build Status](https://travis-ci.org/wjwwood/serial.svg?branch=master)](https://travis-ci.org/wjwwood/serial)*(Linux and OS X)* [![Build Status](https://ci.appveyor.com/api/projects/status/github/wjwwood/serial)](https://ci.appveyor.com/project/wjwwood/serial)*(Windows)*

This is a cross-platform library for interfacing with rs-232 serial like ports written in C++. It provides a modern C++ interface with a workflow designed to look and feel like PySerial, but with the speed and control provided by C++. 

This library is in use in several robotics related projects and can be built and installed to the OS like most unix libraries with make and then sudo make install, but because it is a catkin project it can also be built along side other catkin projects in a catkin workspace.

Serial is a class that provides the basic interface common to serial libraries (open, close, read, write, etc..) and requires no extra dependencies. It also provides tight control over timeouts and control over handshaking lines. 

### Documentation

Website: http://wjwwood.github.io/serial/

API Documentation: http://wjwwood.github.io/serial/doc/1.1.0/index.html

### Dependencies

Required:
* [catkin](http://www.ros.org/wiki/catkin) - cmake and Python based buildsystem
* [cmake](http://www.cmake.org) - buildsystem
* [Python](http://www.python.org) - scripting language
  * [empy](http://www.alcyone.com/pyos/empy/) - Python templating library
  * [catkin_pkg](http://pypi.python.org/pypi/catkin_pkg/) - Runtime Python library for catkin

Optional (for documentation):
* [Doxygen](http://www.doxygen.org/) - Documentation generation tool
* [graphviz](http://www.graphviz.org/) - Graph visualization software

### Install

Get the code:

    git clone https://github.com/wjwwood/serial.git

Build:

    make

Build and run the tests:

    make test

Build the documentation:

    make doc

Install:

    make install

### License

[The MIT License](LICENSE)

### Authors

William Woodall <wjwwood@gmail.com>
John Harrison <ash.gti@gmail.com>

### Contact

William Woodall <william@osrfoundation.org>
