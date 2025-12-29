# Python binding using pybind11

pybind11_add_module(psr_database_py
    src/module.cpp
)

target_link_libraries(psr_database_py
    PRIVATE
        psr_database
        psr_compiler_options
)

set_target_properties(psr_database_py PROPERTIES
    OUTPUT_NAME "_psr_database"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/psr_database"
    # Prevent multi-config generators (MSVC) from adding Release/Debug subdirectories
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/psr_database"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/psr_database"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/psr_database"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/psr_database"
)

# Install Python package files
file(COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/psr_database/__init__.py"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/psr_database"
)
