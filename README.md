rtl-sdr turns your Realtek RTL2832 based DVB dongle into a SDR receiver
======================================================================
Discord support server:  
[![Join Discord server!](https://invidget.switchblade.xyz/HP99wM4yNR)](http://discord.gg/HP99wM4yNR)  
![GitHub all releases](https://img.shields.io/github/downloads/Mr-Precise/rtl-sdr/total?color=%23477CE0&label=Total%20downloads%3A&style=for-the-badge)  

Experimental R820T/R820T2 rtl-sdr tuner driver that tunes down to 13 MHz or lower.  
(From my experiments up to 3.4 MHz on R820T2)

Realtek RTL2832U based hardware can be used as a cheap SDR (Software-defined radio).  
This device allows transferring the raw I/Q samples to the your PC/laptop/etc via USB.

History:  
2017-2018: Backporting features and bug fixes from upstream and newer forks.  
2021: Fully migrated to the CMake build system.  
2022-2023: More fixes, add's, and improvements.  
2023-2026: Driver optimizations, direct sampling improvements, expanded tuner/hardware support, and Modern CMake integration.

## Dependencies
### Linux
Debian/Ubuntu:
```
sudo apt install build-essential pkg-config cmake git libusb-1.0-0-dev
```
Arch/Manjaro:  
Note: package has added to the [AUR](https://aur.archlinux.org/packages/rtl-sdr-exp-git), or you can use PKGBUILD.  
```
pacman -S cmake libusb
```
### Windows
[Visual Studio](https://visualstudio.microsoft.com/) or MinGW/[msys](https://www.msys2.org/)/[LLVM Clang MinGW](https://github.com/mstorsjo/llvm-mingw) etc...  
libusb [libusb/releases](https://github.com/libusb/libusb/releases)  
[pthread-win32 library](https://github.com/GerHobbelt/pthread-win32)  
Latest [CMake](https://cmake.org/download/) or [Old CMake](https://github.com/Kitware/CMake/releases/tag/v3.13.4) for 2003/XP  
Recommended for use: [Precompiled static libs (build kit)](https://github.com/Mr-Precise/SDR-binary-builds-stuff/releases/tag/windows)

### macOS
```
brew install libusb pkg-config cmake
```
## Clone repo

```
git clone https://github.com/Mr-Precise/rtl-sdr/

cd rtl-sdr
```

## Build: linux/macOS
### Configure:
run cmake and start compilation. cmake will accept some options, e.g.
* `-DINSTALL_UDEV_RULES=ON`, default is `OFF`
* `-DDETACH_KERNEL_DRIVER=ON`, default is `OFF`
* `-DENABLE_ZEROCOPY=ON`, default is `OFF`
* `-DLINK_RTL_APPS_WITH_STATIC_LIB=ON`, default is `OFF`
* `-DINSTALL_STATIC_LIB=ON`, default is `ON`

all cmake options are optional  
* `-DCMAKE_INSTALL_PREFIX=/usr` default install prefix on ubuntu.

```
mkdir build && cd build
cmake .. -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON
```
### Build:
using make utility:
```
make -j$(($(nproc) + 1))
```
or build using cmake:
```
cmake --build . --config Release
```

## Install
setup into prefix, usually will require `sudo`:
```
sudo make install
sudo ldconfig
```
or build & install using cmake:
```
cmake --build . --config Release --target install
```
or if you manually build the package from the directory:
```
make install DESTDIR=build_folder_name
```
# Download Release binary builds
[Latest releases](https://github.com/Mr-Precise/rtl-sdr/releases/latest) builds - for Linux (amd64, i386, amrhf, arm64), macOS intel, Windows: (x86 32/64 + ARM64 MSVC and MinGW cross-builds), Android.

# Download Development binary builds

[GitHub Actions](https://github.com/Mr-Precise/rtl-sdr/actions) builds - for Linux amd64 and macOS.  
[Nightly releases](https://github.com/Mr-Precise/rtl-sdr/releases/tag/nightly) builds - for Linux, macOS, Windows: MSVC and MinGW cross-builds.

# Supported Software List
- [Gqrx](https://github.com/gqrx-sdr/gqrx)
- [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus)
- [GNU Radio](https://github.com/gnuradio/gnuradio)
- [OpenWebRX](https://github.com/jketterl/openwebrx)
- SDRSharp
- [CubicSDR](https://github.com/cjcliffe/CubicSDR)
- [HDSDR](https://hdsdr.de/) and other [Winrad](https://www.i2phd.org/winrad/) compatible programs using [ExtIO_RTL](https://github.com/Mr-Precise/ExtIO_RTL)
- [etc](https://www.rtl-sdr.com/big-list-rtl-sdr-supported-software/)  

## Setup for SDR only use - without DVB compatibility:

- a special USB vendor/product id got reserved at http://pid.codes/ : 0x1209/0x2832
- for such devices the linux kernel's DVB modules are not loaded automatically,
  thus can be used without blacklisting dvb_usb_rtl28xxu below /etc/modprobe.d/
- this allows to use a second RTL dongle for use with DVB in parallel
- the IDs can be programmed with 'rtl_eeprom -n' or 'rtl_eeprom -g realtek_sdr'  

## Dev documentation:
### Connect && use library on your project:

#### 1. If using find_package (recommended):
```cmake
find_package(rtlsdr REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    rtlsdr::rtlsdr         # Recommended: automatically selects shared or static
    # rtlsdr::rtlsdr_shared # Or link shared explicitly
    # rtlsdr::rtlsdr_static # Or link static explicitly
)
```

*Classic/legacy CMake variables are also supported:*
```cmake
find_package(rtlsdr REQUIRED)
include_directories(${RTLSDR_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${RTLSDR_LIBRARIES})
```

#### 2. If used as a git submodule:
```cmake
add_subdirectory(rtl-sdr)

# Target names and include directories are fully identical to find_package:
target_link_libraries(${PROJECT_NAME} PRIVATE
    rtlsdr::rtlsdr         # (or rtlsdr::rtlsdr_static / rtlsdr::rtlsdr_shared)
)
```

#### 3. If using pkg-config (pkg_check_modules):
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED librtlsdr)

target_include_directories(${PROJECT_NAME} PRIVATE ${RTLSDR_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${RTLSDR_LIBRARIES})
```

## (old) Notes on the mutability tree (2014/09/29)
Based on developments by Oliver Jowett from [mutability repo](https://github.com/mutability/rtl-sdr)  
### See old [README.old.md](README.old.md) for details
