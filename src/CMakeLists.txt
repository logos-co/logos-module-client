set(CMAKE_AUTOMOC ON)

set(_default_sdk_root "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/logos-cpp-sdk")
if(NOT DEFINED LOGOS_CPP_SDK_ROOT OR LOGOS_CPP_SDK_ROOT STREQUAL "")
  set(LOGOS_CPP_SDK_ROOT "${_default_sdk_root}")
endif()

get_filename_component(LOGOS_CPP_SDK_ROOT "${LOGOS_CPP_SDK_ROOT}" ABSOLUTE)

# Check if we have a built SDK package (with lib and include directories)
if(EXISTS "${LOGOS_CPP_SDK_ROOT}/lib" AND EXISTS "${LOGOS_CPP_SDK_ROOT}/include")
  message(STATUS "Using built logos-cpp-sdk at ${LOGOS_CPP_SDK_ROOT}")

  find_library(LOGOS_SDK_LIBRARY
    NAMES logos_sdk
    PATHS "${LOGOS_CPP_SDK_ROOT}/lib"
    NO_DEFAULT_PATH
  )

  if(NOT LOGOS_SDK_LIBRARY)
    message(FATAL_ERROR "logos_sdk library not found in ${LOGOS_CPP_SDK_ROOT}/lib")
  endif()

  add_library(logos_sdk STATIC IMPORTED)
  set_target_properties(logos_sdk PROPERTIES
    IMPORTED_LOCATION "${LOGOS_SDK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LOGOS_CPP_SDK_ROOT}/include"
  )

elseif(EXISTS "${LOGOS_CPP_SDK_ROOT}/cpp/CMakeLists.txt")
  message(STATUS "Using logos-cpp-sdk source at ${LOGOS_CPP_SDK_ROOT}")
  add_subdirectory(${LOGOS_CPP_SDK_ROOT}/cpp ${CMAKE_BINARY_DIR}/sdk)
else()
  message(FATAL_ERROR "logos-cpp-sdk not found at ${LOGOS_CPP_SDK_ROOT}")
endif()

# Library sources
set(MODULE_CLIENT_SOURCES
    logos_module_client.cpp
    logos_module_client.h
    proxy_api.cpp
    proxy_api.h
    logos_core_client.cpp
    logos_core_client.h
    logos_json_utils.cpp
    logos_json_utils.h
    logos_sdk_c.cpp
    logos_sdk_c.h
)

# Create the library as static
add_library(logos_module_client STATIC ${MODULE_CLIENT_SOURCES})

# Link Qt libraries and SDK
target_link_libraries(logos_module_client PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    logos_sdk
)

# Include directories
target_include_directories(logos_module_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Add SDK include directory if using built SDK
if(EXISTS "${LOGOS_CPP_SDK_ROOT}/lib" AND EXISTS "${LOGOS_CPP_SDK_ROOT}/include")
  target_include_directories(logos_module_client PRIVATE
    ${LOGOS_CPP_SDK_ROOT}/include
    ${LOGOS_CPP_SDK_ROOT}/include/cpp
    ${LOGOS_CPP_SDK_ROOT}/include/core
  )
else()
  target_include_directories(logos_module_client PRIVATE ${LOGOS_CPP_SDK_ROOT}/cpp)
endif()
