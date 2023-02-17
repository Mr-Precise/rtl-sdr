rtl-sdr turns your Realtek RTL2832 based DVB dongle into a SDR receiver
======================================================================
Discord support server:  
[![Join Discord server!](https://invidget.switchblade.xyz/HP99wM4yNR)](http://discord.gg/HP99wM4yNR)  
![GitHub all releases](https://img.shields.io/github/downloads/Mr-Precise/rtl-sdr/total?color=%23477CE0&label=Total%20downloads%3A&style=for-the-badge)  

Experimental R820T/R820T2 rtl-sdr tuner driver that tunes down to 13 MHz or lower.  
(From my experiments up to 3.4 MHz on R820T2)

2017-2018: Transferring/Backporting new features from new versions and fixes...  
2021: Project is now using only cmake build system.  
Integrated RTL_NFC code from https://github.com/Iskuri/RTLSDR-NFC  
2022-2023: more fixes, add's, and improvements.

## Dependencies
### Linux
Debian/Ubuntu:
```
sudo apt install build-essential cmake git libusb-1.0-0-dev
```
Arch/Manjaro:  
Note: there is a PKGBUILD file in the repository and [AUR](https://aur.archlinux.org/packages/rtl-sdr-exp-git)
```
pacman -S cmake libusb
```
### Windows
[Visual Studio](https://visualstudio.microsoft.com/) or MinGW/[msys](https://www.msys2.org/)/[LLVM Clang MinGW](https://github.com/mstorsjo/llvm-mingw) etc...  
libusb [libusb/releases](https://github.com/libusb/libusb/releases)  
[pthread-win32 library](https://github.com/GerHobbelt/pthread-win32)  
Latest [CMake](https://cmake.org/download/) or [Old CMake](https://github.com/Kitware/CMake/releases/tag/v3.13.4) for 2000/XP  
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
make -j$(($(nproc) + 1))
```

## install
setup into prefix, usually will require `sudo`:
```
sudo make install
sudo ldconfig
```
or building a package from a directory
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
- SDRSharp
- [CubicSDR](https://github.com/cjcliffe/CubicSDR)
- [etc](https://www.rtl-sdr.com/big-list-rtl-sdr-supported-software/)  

## Setup for SDR only use - without DVB compatibility:

- a special USB vendor/product id got reserved at http://pid.codes/ : 0x1209/0x2832
- for such devices the linux kernel's DVB modules are not loaded automatically,
  thus can be used without blacklisting dvb_usb_rtl28xxu below /etc/modprobe.d/
- this allows to use a second RTL dongle for use with DVB in parallel
- the IDs can be programmed with 'rtl_eeprom -n' or 'rtl_eeprom -g realtek_sdr'  

## (old) Notes on the mutability tree (2014/09/29)
Based on developments by Oliver Jowett from [mutability repo](https://github.com/mutability/rtl-sdr)  
### See old [README.md](README.old.md) for details
