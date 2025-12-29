# Lua binding

add_library(psr_database_lua SHARED
    src/lua_binding.cpp
)

target_include_directories(psr_database_lua
    PRIVATE
        ${LUA_INCLUDE_DIR}
)

target_link_libraries(psr_database_lua
    PRIVATE
        psr_database
        ${LUA_LIBRARIES}
        psr_compiler_options
)

set_target_properties(psr_database_lua PROPERTIES
    OUTPUT_NAME "psr_database"
    PREFIX ""
)

# Lua modules use .dll on Windows, .so on Unix
if(WIN32)
    set_target_properties(psr_database_lua PROPERTIES SUFFIX ".dll")
else()
    set_target_properties(psr_database_lua PROPERTIES SUFFIX ".so")
endif()
