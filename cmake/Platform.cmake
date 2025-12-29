# Platform-specific configuration

if(WIN32)
    set(PSR_PLATFORM "windows")
    set(PSR_LIB_PREFIX "")
    set(PSR_LIB_SUFFIX ".dll")
    set(PSR_STATIC_LIB_SUFFIX ".lib")
elseif(APPLE)
    set(PSR_PLATFORM "macos")
    set(PSR_LIB_PREFIX "lib")
    set(PSR_LIB_SUFFIX ".dylib")
    set(PSR_STATIC_LIB_SUFFIX ".a")
else()
    set(PSR_PLATFORM "linux")
    set(PSR_LIB_PREFIX "lib")
    set(PSR_LIB_SUFFIX ".so")
    set(PSR_STATIC_LIB_SUFFIX ".a")
endif()

# RPATH settings for Linux/macOS
if(NOT WIN32)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Hide symbols by default (explicit export required)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
