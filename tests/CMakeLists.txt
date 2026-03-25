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
