# Install man pages
set(INSTALL_MAN_PAGES OFF CACHE BOOL "Install man pages")
if(INSTALL_MAN_PAGES)
    # target
    set(MAN_INSTALL_DIR "share/man" CACHE STRING "Target directory for manpages installation.")

    file(GLOB MAN_PAGES "${PROJECT_SOURCE_DIR}/man/*.1")

    foreach(MAN_PAGE ${MAN_PAGES})
        get_filename_component(MAN_NAME ${MAN_PAGE} NAME)
        install(FILES ${MAN_PAGE} DESTINATION ${CMAKE_INSTALL_PREFIX}/${MAN_INSTALL_DIR}/man1 RENAME ${MAN_NAME}.1)
    endforeach()
endif()
