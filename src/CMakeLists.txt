set(CMAKE_AUTOMOC ON)

set(_default_sdk_root "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/logos-cpp-sdk")
if(NOT DEFINED LOGOS_CPP_SDK_ROOT OR LOGOS_CPP_SDK_ROOT STREQUAL "")
  set(LOGOS_CPP_SDK_ROOT "${_default_sdk_root}")
endif()

get_filename_component(LOGOS_CPP_SDK_ROOT "${LOGOS_CPP_SDK_ROOT}" ABSOLUTE)

# The SDK stack since the Qt split: this client layers on the Qt HOST
# RUNTIME (LogosAPI; LogosAPIClient / LogosObject come from the protocol
# layer underneath), which chains logos-protocol (transports + lp_* C ABI)
# with its full transitive link interface — replacing the old hand-rolled
# IMPORTED archive.
#
# The host runtime used to ship from logos-qt-sdk as
# `logos-qt-sdk::logos_qt_sdk`. It now lives in logos-plugin-qt and is
# consumed directly as `logos-qt-host::logos_qt_host` (LOGOS_QT_HOST_ROOT).
# Nothing else in this repo came from logos-qt-sdk — no consumer emitter,
# no LpBridge headers, no UI plugin context — so that dependency is gone
# rather than kept alongside.
# The environment fallbacks go into the CACHE, not this directory's scope:
# -D already lands in the cache, and tests/ (a sibling directory) reads both
# roots. A plain set() here would be invisible there and silently expand to
# an empty include path.
if(NOT DEFINED LOGOS_PROTOCOL_ROOT AND DEFINED ENV{LOGOS_PROTOCOL_ROOT})
  set(LOGOS_PROTOCOL_ROOT "$ENV{LOGOS_PROTOCOL_ROOT}"
      CACHE PATH "Installed logos-protocol prefix")
endif()
if(NOT DEFINED LOGOS_QT_HOST_ROOT AND DEFINED ENV{LOGOS_QT_HOST_ROOT})
  set(LOGOS_QT_HOST_ROOT "$ENV{LOGOS_QT_HOST_ROOT}"
      CACHE PATH "Installed logos-qt-host prefix (logos-plugin-qt)")
endif()

# Each root is checked on its own so the failure names the one that is
# missing. Never turn either of these into a silent skip: a build that
# quietly proceeds without the host runtime fails much later, or worse,
# links a stale copy.
if(NOT EXISTS "${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol")
  message(FATAL_ERROR
    "logos-protocol not found. Set LOGOS_PROTOCOL_ROOT to a built package "
    "(looked for '${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol').")
endif()
if(NOT EXISTS "${LOGOS_QT_HOST_ROOT}/lib/cmake/logos-qt-host")
  message(FATAL_ERROR
    "logos-qt-host not found. Set LOGOS_QT_HOST_ROOT to a built "
    "logos-plugin-qt#logos-qt-host package (looked for "
    "'${LOGOS_QT_HOST_ROOT}/lib/cmake/logos-qt-host').")
endif()

message(STATUS "Using logos-protocol at ${LOGOS_PROTOCOL_ROOT}")
message(STATUS "Using logos-qt-host at ${LOGOS_QT_HOST_ROOT}")
find_package(logos-protocol REQUIRED
  PATHS "${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol" NO_DEFAULT_PATH)
find_package(logos-qt-host REQUIRED
  PATHS "${LOGOS_QT_HOST_ROOT}/lib/cmake/logos-qt-host" NO_DEFAULT_PATH)
if(NOT TARGET logos-qt-host::logos_qt_host)
  message(FATAL_ERROR
    "find_package(logos-qt-host) succeeded but did not define "
    "logos-qt-host::logos_qt_host.")
endif()
if(NOT TARGET logos_sdk)
  add_library(logos_sdk INTERFACE IMPORTED)
  target_link_libraries(logos_sdk INTERFACE logos-qt-host::logos_qt_host)
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
