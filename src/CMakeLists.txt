set(CMAKE_AUTOMOC ON)

set(_default_sdk_root "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/logos-cpp-sdk")
if(NOT DEFINED LOGOS_CPP_SDK_ROOT OR LOGOS_CPP_SDK_ROOT STREQUAL "")
  set(LOGOS_CPP_SDK_ROOT "${_default_sdk_root}")
endif()

get_filename_component(LOGOS_CPP_SDK_ROOT "${LOGOS_CPP_SDK_ROOT}" ABSOLUTE)

# The SDK stack since the Qt split: this client layers on the Qt
# developer layer (logos-qt-sdk: LogosAPI/LogosAPIClient), which chains
# logos-protocol (transports + lp_* C ABI) with its full transitive link
# interface — replacing the old hand-rolled IMPORTED archive.
if(NOT DEFINED LOGOS_PROTOCOL_ROOT AND DEFINED ENV{LOGOS_PROTOCOL_ROOT})
  set(LOGOS_PROTOCOL_ROOT "$ENV{LOGOS_PROTOCOL_ROOT}")
endif()
if(NOT DEFINED LOGOS_QT_SDK_ROOT AND DEFINED ENV{LOGOS_QT_SDK_ROOT})
  set(LOGOS_QT_SDK_ROOT "$ENV{LOGOS_QT_SDK_ROOT}")
endif()

if(EXISTS "${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol"
   AND EXISTS "${LOGOS_QT_SDK_ROOT}/lib/cmake/logos-qt-sdk")
  message(STATUS "Using logos-protocol at ${LOGOS_PROTOCOL_ROOT}")
  message(STATUS "Using logos-qt-sdk at ${LOGOS_QT_SDK_ROOT}")
  find_package(logos-protocol REQUIRED
    PATHS "${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol" NO_DEFAULT_PATH)
  find_package(logos-qt-sdk REQUIRED
    PATHS "${LOGOS_QT_SDK_ROOT}/lib/cmake/logos-qt-sdk" NO_DEFAULT_PATH)
  if(NOT TARGET logos_sdk)
    add_library(logos_sdk INTERFACE IMPORTED)
    target_link_libraries(logos_sdk INTERFACE logos-qt-sdk::logos_qt_sdk)
  endif()
else()
  message(FATAL_ERROR "logos-protocol / logos-qt-sdk not found. Set "
                      "LOGOS_PROTOCOL_ROOT and LOGOS_QT_SDK_ROOT to built packages.")
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

# Create the library as shared (needed for FFI consumers like logos-js-sdk)
add_library(logos_module_client SHARED ${MODULE_CLIENT_SOURCES})
set_target_properties(logos_module_client PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

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
