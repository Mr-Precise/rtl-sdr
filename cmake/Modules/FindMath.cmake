
find_path(MATH_INCLUDE_DIR math.h)

set(MATH_LIB_NAME ${MATH_LIB_NAME} m)
find_library(MATH_LIBRARY NAMES ${MATH_LIB_NAME} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libmath DEFAULT_MSG MATH_LIBRARY MATH_INCLUDE_DIR)

if(libmath_FOUND)
    set(MATH_LIBRARIES ${MATH_LIBRARY})
endif(libmath_FOUND)
