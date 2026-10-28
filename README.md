# Serial Communication Library — CogaF fork

A C++ cross-platform serial-port library. This repository is a fork of
[wjwwood/serial](https://github.com/wjwwood/serial) that keeps the
original API (and its MIT License) and adds a set of practical
improvements that are useful for day-to-day use with real USB-serial
hardware. All credits for the design and the bulk of the implementation
go to the original authors; see the [Original authors](#original-authors)
section at the bottom.

The fork lives at
[CogaF/serial](https://github.com/CogaF/serial). Open issues or pull
requests there, not on the upstream project.

## What this fork changes

### Earlier fork modifications (pre-2.0, kept)

* `Serial::getPort()` returns `std::wstring` instead of `std::string`,
  to silence a wchar/char narrowing warning in builds that mix wide and
  narrow strings.
* `Serial::open()` no longer throws when the port is already open; it
  closes and reopens it instead. The original behaviour (an exception)
  was not useful in the author's own code paths.

### 2.0 additions

All additions below are **source and binary backward compatible**:
every existing method signature in `serial::Serial` and every existing
field of `serial::PortInfo` keeps its position and type. Code written
against the pre-fork headers compiles against this version unchanged.

**New methods on `serial::Serial`**

| Method | What it does |
|---|---|
| `cancelRead()` | Unblocks a blocking `read()` from another thread. Windows: `CancelIoEx`. Linux: an eventfd multiplexed into the read loop. |
| `getLineErrors()` | Returns and clears a bitmask of frame / parity / overrun / break / buffer-overflow errors accumulated since the last call (`serial::LineError`). |
| `waitForModemChange(timeout_ms)` | Bounded wait for a change on CTS / DSR / RI / CD. |
| `setRS485Mode(enabled, before_us, after_us)` / `getRS485Mode()` | Linux: `TIOCSRS485`. Windows: `RTS_CONTROL_TOGGLE`. macOS: no-op. |
| `setLowLatency(enabled)` | Linux `ASYNC_LOW_LATENCY` via `TIOCGSERIAL` / `TIOCSSERIAL`. Windows is a documented no-op (FTDI `LatencyTimer` registry key needs elevation and is site-specific). |
| `setExclusive(enabled)` | Linux: `TIOCEXCL` / `TIOCNXCL`. Windows: already exclusive by default (no-op). |
| `setXonXoffChars(xon, xoff)` | Settable XON / XOFF characters (Windows: `DCB.XonChar` / `XoffChar`; POSIX: `c_cc[VSTART]` / `[VSTOP]`). |
| `setDriverBufferSize(in, out)` | Windows: `SetupComm`. POSIX: no-op. On Windows the library already asks the driver for 64 KiB in / 64 KiB out on `open()`. |

**New namespace-level functions**

| Function | What it does |
|---|---|
| `registerHotplugCallback(cb)` / `unregisterHotplugCallback(token)` | Background monitor for port insertions / removals. Linux uses `libudev` when available (returns `-1` otherwise). Windows and macOS use a `list_ports()` polling worker. |
| `version()`, `version_major()`, `version_minor()`, `version_patch()`, `version_commits()` | Report `"2.0.0+<commit-count>"` as computed at build time. |

**`PortInfo` enrichment** — existing fields (`port`, `description`,
`hardware_id`) keep their position and type. The new fields are
appended at the end so brace-initialization of existing code is not
affected:

* `vid`, `pid` — hex strings (e.g. `"0403"`, `"6001"`).
* `serial_number`
* `manufacturer`
* `product`
* `location` — platform-specific USB location string.
* `driver` — driver / service name.

They are populated from sysfs (Linux), `SetupDiGetDeviceRegistryProperty`
(Windows), and IOKit (macOS).

**Windows-only fixes** — the following methods used to throw
`"not implemented"` and now work:

* `waitReadable()` — polls `ClearCommError` within the configured
  read timeout.
* `waitByteTimes(count)` — sleeps for the transmission time of
  `count` bytes at the current line settings.
* `sendBreak(duration_ms)` — `SetCommBreak` / `Sleep` / `ClearCommBreak`.

**Build-system changes**

* Modern CMake (`cmake_minimum_required 3.15`), C++17, install / export
  through `serialTargets.cmake`.
* The catkin block is retained inside a `CATKIN_DEVEL_PREFIX` guard, so
  ROS 1 workspaces still build.
* Optional `libudev` detection on Linux via `pkg_check_modules`; when
  present, the `SERIAL_HAVE_LIBUDEV` macro is defined and the
  libudev-based hotplug backend is used.
* `GitHub Actions` workflow for Linux (with `libudev-dev` and
  pty-based tests) and Windows (build only — `com0com` cannot be
  installed on hosted runners).

### DLL build on Windows

The library now exports a `SERIAL_API` macro so it can be built as a
DLL on Windows:

* The serial project itself defines `SERIAL_BUILD_DLL` in its
  `Debug_DLL` / `Release_DLL` configurations.
* Consumers that link against the DLL must `#define SERIAL_USE_DLL`
  before including `<serial/serial.h>` (or add it as a preprocessor
  definition in the consuming project).
* Static builds define neither macro and link exactly as before.

### Published artifact layout

Builds publish their outputs under
`<repo>/serial_lib_structure/` using a layout downstream projects
(`peltier_cntl_tec_8a` and others) can resolve directly:

```
serial_lib_structure/
├── include/
│   └── serial/
│       ├── serial.h, v8stdint.h
│       └── impl/{unix.h, win.h, win_compat.h, hotplug.h}
├── x64/
│   ├── Debug/
│   │   ├── lib/serial.lib
│   │   └── dll/{serial.lib, serial.dll, serial.pdb}
│   └── Release/
│       ├── lib/serial.lib
│       └── dll/{serial.lib, serial.dll, serial.pdb}
└── x86/
    ├── Debug/
    │   ├── lib/serial.lib
    │   └── dll/{serial.lib, serial.dll, serial.pdb}
    └── Release/
        ├── lib/serial.lib
        └── dll/{serial.lib, serial.dll, serial.pdb}
```

Point the environment variable `VC_WJWWOOD_SERIAL` at the
`serial_lib_structure/` folder and downstream projects that follow the
`$(VC_WJWWOOD_SERIAL)\$(PlatformShortName)\$(DepConfig)\$(SerialLinkKind)`
convention will resolve `serial.lib` and `serial.dll` automatically.

## Dependencies

**Required**

* A C++17 compiler.
* [CMake](https://cmake.org) 3.15 or newer (standalone / Linux / macOS /
  CI builds) **or** Visual Studio 2017 or newer (Windows IDE build).

**Optional**

* [libudev](https://man7.org/linux/man-pages/man3/udev.3.html) — enables
  rich hotplug events on Linux. Package name: `libudev-dev` on Debian /
  Ubuntu, `systemd-devel` on Fedora.
* [Doxygen](https://www.doxygen.nl/) + [graphviz](https://graphviz.org/)
  — API documentation.
* [catkin](https://wiki.ros.org/catkin) — only if building inside a
  ROS 1 workspace; the shim stays dormant otherwise.

## Build

### Linux / macOS (CMake)

```sh
git clone https://github.com/CogaF/serial.git
cd serial

# Static library (default). Debug variant: -DCMAKE_BUILD_TYPE=Debug.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Shared library (same tree).
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-shared -j
```

Output lands under `serial_lib_structure/<x64|x86>/<Debug|Release>/<lib|dll>/`.
Public headers are copied under `serial_lib_structure/include/serial/`
on every successful build.

### Linux / macOS (classic `make`)

The repository ships a top-level `Makefile` that wraps CMake for
convenience:

```sh
make           # configure + build (Release, static)
make test      # build and run the test suite
make doc       # build the Doxygen documentation
sudo make install
```

### Windows (Visual Studio)

Open `visual_studio\visual_studio.sln` in Visual Studio 2017 or newer.
Eight configurations are available for the `serial` project:

| Configuration | Platform | Produces |
|---|---|---|
| `Debug` | `Win32` / `x64` | static `serial.lib` under `x86\Debug\lib\` or `x64\Debug\lib\` |
| `Release` | `Win32` / `x64` | static `serial.lib` under `x86\Release\lib\` or `x64\Release\lib\` |
| `Debug_DLL` | `Win32` / `x64` | `serial.dll` + import `serial.lib` under `x86\Debug\dll\` or `x64\Debug\dll\` |
| `Release_DLL` | `Win32` / `x64` | `serial.dll` + import `serial.lib` under `x86\Release\dll\` or `x64\Release\dll\` |

Build `Rebuild All` (or `Batch Build…` to produce every variant at once).
The post-build step copies the public headers under
`serial_lib_structure\include\serial\`.

### Windows (CMake / command-line MSVC)

```powershell
# Static
cmake -S . -B build-static -G "Visual Studio 17 2022" -A x64
cmake --build build-static --config Release -j

# Shared
cmake -S . -B build-shared -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=ON
cmake --build build-shared --config Release -j
```

### ROS 1 (catkin)

Drop this folder into your catkin workspace's `src/` and build with
`catkin_make` / `catkin build` as usual. The catkin shim auto-engages
when `CATKIN_DEVEL_PREFIX` is set.

## Test

* **Linux**: pty-based tests under `tests/pty_test.cc`. Enable with
  `-DBUILD_TESTING=ON`, then `ctest --output-on-failure` inside the
  build directory, or `make test`.
* **Windows**: `com0com` is required for serial-pair tests but cannot
  be installed on CI runners. On a workstation with `com0com` set up,
  the test binary at `visual_studio\<Platform>\<Configuration>\test_serial.exe`
  can be run against a `com0com` port pair.
* **CI**: `.github/workflows/ci.yml` runs the Linux build + tests and
  the Windows build on every push.

## Use in your own project

### CMake consumer (any platform)

```cmake
find_package(serial 2.0 CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE serial::serial)
```

### Visual Studio consumer (static)

1. Set the environment variable `VC_WJWWOOD_SERIAL` to the absolute
   path of this fork's `serial_lib_structure` folder. Restart Visual
   Studio so the new value is picked up.
2. In your project's property pages:
   * `C/C++ → General → Additional Include Directories` add
     `$(VC_WJWWOOD_SERIAL)\include`.
   * `Linker → General → Additional Library Directories` add
     `$(VC_WJWWOOD_SERIAL)\$(PlatformShortName)\$(Configuration)\lib`
     (or wire it to a `DepConfig` macro that maps `Release_DLL` →
     `Release` for your own `_DLL` configurations, as
     `peltier_cntl_tec_8a` does).
   * `Linker → Input → Additional Dependencies` add `serial.lib`.

### Visual Studio consumer (DLL)

Same as above but:

* Use the `dll` subfolder instead of `lib`.
* Define `SERIAL_USE_DLL` in `C/C++ → Preprocessor → Preprocessor Definitions`.
* Add a post-build step that copies `serial.dll` next to your
  executable:
  ```
  xcopy /y /d "$(VC_WJWWOOD_SERIAL)\$(PlatformShortName)\$(DepConfig)\dll\serial.dll" "$(OutDir)"
  ```

### Minimal example

```cpp
#include <serial/serial.h>
#include <iostream>

int main()
{
    for (const auto& p : serial::list_ports()) {
        std::cout << p.port << " "
                  << p.description << " "
                  << p.vid << ":" << p.pid << " "
                  << p.serial_number << '\n';
    }

    serial::Serial s("COM3", 115200,
                     serial::Timeout::simpleTimeout(1000));
    s.write("AT\r\n");
    std::cout << s.readline() << '\n';
}
```

## Documentation

* Fork repository: <https://github.com/CogaF/serial>
* Upstream documentation (API shape is identical for the shared surface):
  <http://wjwwood.github.io/serial/doc/1.1.0/index.html>

## License

[The MIT License](LICENSE) — unchanged from upstream.

## Original authors

The library was created and is designed by:

* William Woodall &lt;wjwwood@gmail.com&gt; — <https://github.com/wjwwood>
* John Harrison &lt;ash.gti@gmail.com&gt;

The copyright and the licence notice (`LICENSE`) are preserved as
required by the MIT terms. This fork does not transfer any authorship
over the pre-existing code; it only maintains additional patches on top
of it. New files introduced by this fork follow the same MIT licence.

## Fork maintainer

Coga Fation &lt;coga.fation@gmail.com&gt;
