set(CPACK_PACKAGE_NAME "rtl-sdr")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_DEBIAN_PACKAGE_NAME "librtlsdr0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Software defined radio receiver for Realtek RTL2832U")

# Debian packaging settings
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Experimental R820T/R820T2 rtl-sdr tuner driver that tunes down to 13 MHz or lower.
(From my experiments up to 3.4 MHz on R820T2)\n
Realtek RTL2832U based hardware can be used as a cheap SDR (Software-defined radio).\n
This device allows transferring the raw I/Q samples to the your PC/laptop/etc via USB.\n
This package contains the binaries, shared and dev library.") 

set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Mr-Precise/rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Precise")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "librtlsdr-dev, rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "librtlsdr-dev, rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_REPLACES "librtlsdr-dev, rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

# Directory for generated Packages
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/Generated_Packages")

# Windows NSIS packaging settings
set(DEFAULT_ICON "${PROJECT_SOURCE_DIR}/src/icon.ico")
set(CPACK_NSIS_MUI_ICON "${DEFAULT_ICON}")
set(CPACK_NSIS_INSTALLED_ICON_NAME "${DEFAULT_ICON}")
set(CPACK_NSIS_DELETE_ICON_NAME "${DEFAULT_ICON}")
set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_DEBIAN_PACKAGE_HOMEPAGE}")
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_BRANDING_TEXT "")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_MENU_LINKS
  "${CPACK_DEBIAN_PACKAGE_HOMEPAGE}" "Visit homepage (github)"
)

# set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

# For generating the correct filename depending on which platform
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(ARCHITECTURE_NAME "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
        set(ARCHITECTURE_NAME "i386")
    else()
        set(ARCHITECTURE_NAME ${CMAKE_SYSTEM_PROCESSOR})
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(ARCHITECTURE_NAME "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
        set(ARCHITECTURE_NAME "i386")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(ARCHITECTURE_NAME "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
        set(ARCHITECTURE_NAME "armhf")
    else()
        set(ARCHITECTURE_NAME ${CMAKE_SYSTEM_PROCESSOR})
    endif()
endif()

# set name for output packages
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${ARCHITECTURE_NAME}.deb")
set(CPACK_NSIS_EXECUTABLE_NAME "{CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${ARCHITECTURE_NAME}.exe")
# set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Generating a package with debug information provided that build type Debug or RelWithDebInfo.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
# else()
    # set(CPACK_STRIP_FILES ON)
endif()

include(CPack)
