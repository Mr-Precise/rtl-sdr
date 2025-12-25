# Copyright 2012 OSMOCOM Project
#
# This file is part of rtl-sdr
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.


########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 3.7.2)

# workaround for https://gitlab.kitware.com/cmake/cmake/issues/16967
if(${CMAKE_VERSION} VERSION_LESS "3.12.0")
    project(rtlsdr)
else()
    project(rtlsdr C)
endif()

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

# Set paths early according to GNUInstallDirs
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix \${prefix})
set(includedir \${prefix}/${CMAKE_INSTALL_INCLUDEDIR})
set(libdir \${exec_prefix}/${CMAKE_INSTALL_LIBDIR})

# Set the version information here
set(VERSION_INFO_MAJOR_VERSION 2024) # increment major on api compatibility changes
set(VERSION_INFO_MINOR_VERSION 1) # increment minor on feature-level changes
set(VERSION_INFO_PATCH_VERSION git) # increment patch for bug fixes and docs
include(Version) # setup version info
include(CpackPacking)

########################################################################
# Compiler specific setup
########################################################################
if(CMAKE_COMPILER_IS_GNUCC AND NOT WIN32)
    add_definitions(-Wall)
    add_definitions(-Wextra)
    add_definitions(-Wno-unused-parameter)
    add_definitions(-Wno-unused)
    add_definitions(-Wsign-compare)
    add_definitions(-Wdeclaration-after-statement)
    #http://gcc.gnu.org/wiki/Visibility
    add_definitions(-fvisibility=hidden)
elseif(MSVC14 OR MSVC14)
#pthread-w32 issue, timespec is now part of time.h
    add_definitions(-D_TIMESPEC_DEFINED)
endif()

########################################################################
# Find build dependencies
########################################################################
find_package(Threads)
find_package(PkgConfig)

include(FindMath)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBUSB libusb-1.0 IMPORTED_TARGET)
    if(LIBUSB_LINK_LIBRARIES)
        set(LIBUSB_LIBRARIES "${LIBUSB_LINK_LIBRARIES}")
    endif()
else()
    set(LIBUSB_LIBRARIES "" CACHE STRING "manual libusb path")
    set(LIBUSB_INCLUDE_DIRS "" CACHE STRING "manual libusb includepath")
endif()

if(MSVC)
    set(THREADS_PTHREADS_LIBRARY "" CACHE STRING "manual pthread-win32 path")
    set(THREADS_PTHREADS_INCLUDE_DIR "" CACHE STRING "manual pthread-win32 includepath")
else()
    set(THREADS_PTHREADS_LIBRARY "" CACHE INTERNAL "manual pthread-win32 path")
    set(THREADS_PTHREADS_INCLUDE_DIR "" CACHE INTERNAL "manual pthread-win32 includepath")
endif()

if(PKG_CONFIG_FOUND AND NOT LIBUSB_FOUND)
    message(FATAL_ERROR "LibUSB 1.0 required to compile rtl-sdr")
endif()
if(NOT THREADS_FOUND)
    message(FATAL_ERROR "pthreads(-win32) required to compile rtl-sdr")
endif()

########################################################################
# Confirure version info header
########################################################################
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/version.h
    @ONLY
)

########################################################################
# Create uninstall target
########################################################################
configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
@ONLY)

add_custom_target(uninstall
    ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

########################################################################
# Install udev rules
########################################################################
option(INSTALL_UDEV_RULES "Install udev rules for RTL-SDR" OFF)
if (INSTALL_UDEV_RULES)
set(UDEV_RULES_PATH "/etc/udev/rules.d" CACHE STRING "Target directory for udev rule installation.")
    install (
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/rtl-sdr.rules"
        DESTINATION ${UDEV_RULES_PATH}
        COMPONENT "udev"
    )
else (INSTALL_UDEV_RULES)
    message (STATUS "Udev rules not being installed, install them with -DINSTALL_UDEV_RULES=ON")
endif (INSTALL_UDEV_RULES)

########################################################################
# Detach kernel driver
########################################################################
option(DETACH_KERNEL_DRIVER "Detach kernel driver if loaded" OFF)
if (DETACH_KERNEL_DRIVER)
    message (STATUS "Building with kernel driver detaching enabled")
    add_definitions(-DDETACH_KERNEL_DRIVER=1)
else (DETACH_KERNEL_DRIVER)
    message (STATUS "Building with kernel driver detaching disabled, use -DDETACH_KERNEL_DRIVER=ON to enable")
endif (DETACH_KERNEL_DRIVER)

########################################################################
# usbfs zero-copy support
########################################################################
option(ENABLE_ZEROCOPY "Enable usbfs zero-copy support" OFF)
if (ENABLE_ZEROCOPY)
    message (STATUS "Building with usbfs zero-copy support enabled")
    add_definitions(-DENABLE_ZEROCOPY=1)
else (ENABLE_ZEROCOPY)
    message (STATUS "Building with usbfs zero-copy support disabled, use -DENABLE_ZEROCOPY=ON to enable")
endif (ENABLE_ZEROCOPY)

########################################################################
# rtl-apps link library options
########################################################################
option(LINK_RTL_APPS_WITH_STATIC_LIB "Link statilally rtl-apps with librtlsdr" OFF)
if(LINK_RTL_APPS_WITH_STATIC_LIB)
    message(STATUS "ENABLED statilally link rtl-apps with librtlsdr")
else(LINK_RTL_APPS_WITH_STATIC_LIB)
    message(STATUS "DISABLED rtl-apps statically link with librtlsdr")
endif(LINK_RTL_APPS_WITH_STATIC_LIB)

########################################################################
# install static lib options
########################################################################
option(INSTALL_STATIC_LIB "Install static librtlsdr" ON)
if(INSTALL_STATIC_LIB)
    message(STATUS "ENABLED install static librtlsdr (default ON)")
else(INSTALL_STATIC_LIB)
    message(STATUS "DISABLED lnstall static librtlsdr")
endif(INSTALL_STATIC_LIB)

########################################################################
# Install public header files
########################################################################
install(FILES
    include/rtl-sdr.h
    include/rtl-sdr_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

########################################################################
# Add subdirectories
########################################################################
add_subdirectory(src)

########################################################################
# Create Pkg Config File
########################################################################
foreach(inc ${LIBUSB_INCLUDEDIR})
    list(APPEND RTLSDR_PC_CFLAGS "-I${inc}")
endforeach(inc)

foreach(lib ${LIBUSB_LIBRARY_DIRS})
    list(APPEND RTLSDR_PC_LIBS "-L${lib}")
endforeach(lib)

# use space-separation format for the pc file
string(REPLACE ";" " " RTLSDR_PC_CFLAGS "${RTLSDR_PC_CFLAGS}")
string(REPLACE ";" " " RTLSDR_PC_LIBS "${RTLSDR_PC_LIBS}")

# unset these vars to avoid hard-coded paths to cross environment
if(CMAKE_CROSSCOMPILING)
    unset(RTLSDR_PC_CFLAGS)
    unset(RTLSDR_PC_LIBS)
endif(CMAKE_CROSSCOMPILING)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/librtlsdr.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/librtlsdr.pc
@ONLY)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/librtlsdr.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

########################################################################
# Create CMake Config File
########################################################################
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/rtlsdr/rtlsdrConfigVersion.cmake"
  VERSION ${VERSION}
  COMPATIBILITY AnyNewerVersion
  )

configure_file(cmake/rtlsdrConfig.cmake
  "${CMAKE_CURRENT_BINARY_DIR}/rtlsdr/rtlsdrConfig.cmake"
  COPYONLY
  )

set(ConfigPackageLocation lib/cmake/rtlsdr)
install(EXPORT RTLSDR-export
  FILE rtlsdrTargets.cmake
  NAMESPACE rtlsdr::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtlsdr/
  )
install(
  FILES
  cmake/rtlsdrConfig.cmake
  "${CMAKE_CURRENT_BINARY_DIR}/rtlsdr/rtlsdrConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rtlsdr/
  COMPONENT Devel
  )

########################################################################
# Print Summary
########################################################################
message(STATUS "Building for version: ${VERSION} / ${LIBVER}")
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")
