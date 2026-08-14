# Module Client Tests

add_executable(module_client_tests
    test_main.cpp
    test_proxy_api.cpp
    test_logos_json_utils.cpp
    test_logos_core_client.cpp
    test_logos_sdk_c.cpp
)

target_link_libraries(module_client_tests PRIVATE
    logos_module_client
    GTest::gtest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
)

target_include_directories(module_client_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Mock-transport headers now ship from logos-protocol; the Qt host-runtime
# headers (logos_api.h) from logos-plugin-qt's logos-qt-host.
#
# Unconditional on purpose. src/CMakeLists.txt is added first (see the top
# level) and hard-errors unless both roots point at built packages, so by
# the time this runs they are known good — the old `if(DEFINED ... AND
# EXISTS ...)` guards could only ever hide a misconfiguration, dropping the
# include path and failing later with a confusing "no such file".
target_include_directories(module_client_tests PRIVATE
  ${LOGOS_PROTOCOL_ROOT}/include
  ${LOGOS_PROTOCOL_ROOT}/include/cpp
  ${LOGOS_PROTOCOL_ROOT}/include/cpp/implementations/mock
  ${LOGOS_QT_HOST_ROOT}/include
  ${LOGOS_QT_HOST_ROOT}/include/cpp
)

# Add SDK mock include directory for test_logos_core_client and test_logos_sdk_c
if(EXISTS "${LOGOS_CPP_SDK_ROOT}/lib" AND EXISTS "${LOGOS_CPP_SDK_ROOT}/include")
  target_include_directories(module_client_tests PRIVATE
    ${LOGOS_CPP_SDK_ROOT}/include
    ${LOGOS_CPP_SDK_ROOT}/include/cpp
    ${LOGOS_CPP_SDK_ROOT}/include/cpp/implementations/mock
  )
else()
  target_include_directories(module_client_tests PRIVATE
    ${LOGOS_CPP_SDK_ROOT}/cpp
    ${LOGOS_CPP_SDK_ROOT}/cpp/implementations/mock
  )
endif()

gtest_discover_tests(module_client_tests)
