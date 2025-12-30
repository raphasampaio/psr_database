add_library(psr_database_c SHARED
    psr_database.cpp
    psr_result.cpp
)

target_compile_definitions(psr_database_c
    PRIVATE
        PSR_DATABASE_C_EXPORTS
        PSR_VERSION="${PROJECT_VERSION}"
)

target_include_directories(psr_database_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(psr_database_c
    PRIVATE
        psr_database
        psr_compiler_options
)

set_target_properties(psr_database_c PROPERTIES
    OUTPUT_NAME psr_database_c
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Alias for use in subdirectories
add_library(psr::database_c ALIAS psr_database_c)

# Installation rules
include(GNUInstallDirs)
install(TARGETS psr_database_c
    EXPORT psr_databaseTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/psr_database.h
    ${CMAKE_CURRENT_SOURCE_DIR}/psr_result.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
