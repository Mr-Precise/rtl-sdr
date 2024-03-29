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
2017-2018: Transferring/Backporting new features from new versions and fixes...  
2021: Project is now using only cmake build system.  
Integrated RTL_NFC code from https://github.com/Iskuri/RTLSDR-NFC  
2022-2023: more fixes, add's, and improvements.

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
Recommented for use: [Precompiled static libs (build kit)](https://github.com/Mr-Precise/SDR-binary-builds-stuff/releases/tag/windows)

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

## install
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

# Supportled Software List
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
If using find_package:
```cmake
find_package(rtlsdr REQUIRED)

target_link_libraries(${PROJECT_NAME}
    rtlsdr::rtlsdr_static #static library
    rtlsdr::rtlsdr_shared #shared library
)
```
If using pkg_check_modules:
```cmake
find_package(PkgConfig)
pkg_check_modules(RTLSDR librtlsdr REQUIRED)

include_directories(${RTLSDR_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    ${RTLSDR_LIBRARIES}
)
```
If used as a git submodule:
```cmake
add_subdirectory(rtl-sdr)
include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl-sdr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl-sdr/src
)

target_link_libraries(${PROJECT_NAME}
    rtlsdr_shared
    rtlsdr_static #for static link
    convenience_static #only for static link
)
```

## (old) Notes on the mutability tree (2014/09/29)
Based on developments by Oliver Jowett from [mutability repo](https://github.com/mutability/rtl-sdr)  
### See old [README.md](README.old.md) for details
