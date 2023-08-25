# Copyright 2012-2020 Osmocom Project
#
# This file is part of rtl-sdr
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

########################################################################
# rtl-sdr sources
########################################################################
set(rtlsdr_SOURCES
    librtlsdr.c
    tuner_e4k.c
    tuner_fc0012.c
    tuner_fc0013.c
    tuner_fc2580.c
    tuner_max2112.c
    tuner_r82xx.c)

########################################################################
# Set up Windows DLL/EXE resource files
########################################################################
if(MSVC OR MINGW)
  include(${PROJECT_SOURCE_DIR}/cmake/Modules/Version.cmake)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/rtlsdr.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/rtlsdr.rc
    @ONLY)

    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/rtl_apps.rc.in
      ${CMAKE_CURRENT_BINARY_DIR}/rtl_apps.rc
      @ONLY)
  
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/icon.ico
      ${CMAKE_CURRENT_BINARY_DIR}/icon.ico
      COPYONLY)
  
      set(librtlsdr_RES ${CMAKE_CURRENT_BINARY_DIR}/rtlsdr.rc)
      set(rtl_apps_RES ${CMAKE_CURRENT_BINARY_DIR}/rtl_apps.rc)
endif(MSVC OR MINGW)

########################################################################
# Setup shared library variant
########################################################################
add_library(rtlsdr_shared SHARED ${rtlsdr_SOURCES} ${librtlsdr_RES})
target_link_libraries(rtlsdr_shared ${LIBUSB_LIBRARIES} ${THREADS_PTHREADS_LIBRARY} ${MATH_LIBRARIES})
if(APPLE)
  target_link_directories(rtlsdr_shared PUBLIC ${LIBUSB_LIBRARY_DIRS})
endif(APPLE)

target_include_directories(rtlsdr_shared PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
  ${LIBUSB_INCLUDE_DIRS}
  ${THREADS_PTHREADS_INCLUDE_DIR}
  )
set_target_properties(rtlsdr_shared PROPERTIES DEFINE_SYMBOL "rtlsdr_EXPORTS")
set_target_properties(rtlsdr_shared PROPERTIES OUTPUT_NAME rtlsdr)
# set_target_properties(rtlsdr PROPERTIES SOVERSION ${MAJOR_VERSION})
# Very crutch fix for compatibility with 0.x.x versions
set_target_properties(rtlsdr_shared PROPERTIES SOVERSION "0")
set_target_properties(rtlsdr_shared PROPERTIES VERSION ${LIBVER})
generate_export_header(rtlsdr_shared)

########################################################################
# Setup static library variant
########################################################################
add_library(rtlsdr_static STATIC ${rtlsdr_SOURCES})
target_link_libraries(rtlsdr_static ${LIBUSB_LIBRARIES} ${THREADS_PTHREADS_LIBRARY} ${MATH_LIBRARIES})
if(LINK_RTL_APPS_WITH_STATIC_LIB)
  if(APPLE)
    target_link_directories(rtlsdr_static PUBLIC ${LIBUSB_LIBRARY_DIRS})
  endif(APPLE)
endif()
target_include_directories(rtlsdr_static PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
  ${LIBUSB_INCLUDE_DIRS}
  ${THREADS_PTHREADS_INCLUDE_DIR}
  )
set_property(TARGET rtlsdr_static APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
if(NOT WIN32)
  # Force same library filename for static and shared variants of the library
  set_target_properties(rtlsdr_static PROPERTIES OUTPUT_NAME rtlsdr)
endif()
generate_export_header(rtlsdr_static)

########################################################################
# link apps settings
########################################################################
if(LINK_RTL_APPS_WITH_STATIC_LIB)
    set(RTL_APP_SELECT_LIB rtlsdr_static)
else()
    set(RTL_APP_SELECT_LIB rtlsdr_shared)
endif()

########################################################################
# Setup libraries used in executables
########################################################################
add_library(convenience_static STATIC
    convenience/convenience.c
)
target_include_directories(convenience_static
  PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
  add_library(libgetopt_static STATIC
    getopt/getopt.c
  )
target_link_libraries(convenience_static
  ${RTL_APP_SELECT_LIB}
  )
endif()

########################################################################
# Build utility
########################################################################
add_executable(rtl_sdr rtl_sdr.c ${rtl_apps_RES})
add_executable(rtl_tcp rtl_tcp.c ${rtl_apps_RES})
add_executable(rtl_test rtl_test.c ${rtl_apps_RES})
add_executable(rtl_fm rtl_fm.c ${rtl_apps_RES})
add_executable(rtl_ir rtl_ir.c ${rtl_apps_RES})
add_executable(rtl_eeprom rtl_eeprom.c ${rtl_apps_RES})
add_executable(rtl_adsb rtl_adsb.c ${rtl_apps_RES})
add_executable(rtl_power rtl_power.c ${rtl_apps_RES})
add_executable(rtl_biast rtl_biast.c ${rtl_apps_RES})
add_executable(rtl_nfc rtl_nfc.c ${rtl_apps_RES})

target_link_libraries(rtl_sdr ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(rtl_tcp ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(rtl_test ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)
target_link_libraries(rtl_fm ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)
target_link_libraries(rtl_ir ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)
target_link_libraries(rtl_eeprom ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(rtl_adsb ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)
target_link_libraries(rtl_power ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)
target_link_libraries(rtl_biast ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
target_link_libraries(rtl_nfc ${RTL_APP_SELECT_LIB} convenience_static
    ${LIBUSB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MATH_LIBRARIES}
)

# static link winpthread for MinGW build
# TODO Rewrite/Remove/Fix
if(MINGW)
  target_link_libraries(rtl_tcp -static winpthread)
  target_link_libraries(rtl_fm -static winpthread)
  target_link_libraries(rtl_adsb -static winpthread)
endif()

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "Haiku")
    target_link_libraries(rtl_tcp network)
  endif()
  if(APPLE OR CMAKE_SYSTEM MATCHES "OpenBSD" OR ANDROID)
    target_link_libraries(rtl_test ${MATH_LIBRARIES})
  endif()
endif()

if(WIN32)
  target_link_libraries(rtl_sdr libgetopt_static)
  target_link_libraries(rtl_tcp ws2_32 libgetopt_static)
  target_link_libraries(rtl_test libgetopt_static)
  target_link_libraries(rtl_fm libgetopt_static)
  target_link_libraries(rtl_ir libgetopt_static)
  target_link_libraries(rtl_eeprom libgetopt_static)
  target_link_libraries(rtl_adsb libgetopt_static)
  target_link_libraries(rtl_power libgetopt_static)
  target_link_libraries(rtl_biast libgetopt_static)
  target_link_libraries(rtl_nfc libgetopt_static)
  set_property(TARGET rtl_sdr APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_tcp APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_test APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_fm APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_ir APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_eeprom APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_adsb APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_power APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_biast APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
  set_property(TARGET rtl_nfc APPEND PROPERTY COMPILE_DEFINITIONS "rtlsdr_STATIC" )
endif()
########################################################################
# Install built library files & utilities
########################################################################
set(INSTALL_TARGETS_UTILS rtl_sdr rtl_tcp rtl_test rtl_fm rtl_ir rtl_eeprom rtl_adsb rtl_power rtl_biast rtl_nfc)

install(TARGETS rtlsdr_shared EXPORT RTLSDR-export
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so/.dylib file
)
if(INSTALL_STATIC_LIB)
  install(TARGETS rtlsdr_static EXPORT RTLSDR-export
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # .a/.lib file
  )
endif()
install(TARGETS ${INSTALL_TARGETS_UTILS}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
