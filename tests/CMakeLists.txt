include(GoogleTest)

add_executable(psr_database_tests
    test_database.cpp
    test_result.cpp
)

target_link_libraries(psr_database_tests
    PRIVATE
        psr_database
        GTest::gtest_main
        GTest::gmock
)

gtest_discover_tests(psr_database_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# C API tests
if(PSR_BUILD_C_API)
    add_executable(psr_database_c_tests
        test_c_api.cpp
    )

    target_link_libraries(psr_database_c_tests
        PRIVATE
            psr_database_c
            GTest::gtest_main
    )

    gtest_discover_tests(psr_database_c_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
