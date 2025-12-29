# Language bindings

if(PSR_BUILD_PYTHON_BINDING)
    add_subdirectory(python)
endif()

if(PSR_BUILD_LUA_BINDING)
    add_subdirectory(lua)
endif()
