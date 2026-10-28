if(CATKIN_ENABLE_TESTING)
    if(UNIX)
        catkin_add_gtest(${PROJECT_NAME}-test unix_serial_tests.cc)
        target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
        if(NOT APPLE)
            target_link_libraries(${PROJECT_NAME}-test util)
        endif()

        if(NOT APPLE)  # these tests are unreliable on macOS
          catkin_add_gtest(${PROJECT_NAME}-test-timer unit/unix_timer_tests.cc)
          target_link_libraries(${PROJECT_NAME}-test-timer ${PROJECT_NAME})
        endif()
    endif()
endif()

# Standalone (non-catkin) pty-based tests, Linux only.
if(UNIX AND NOT APPLE AND BUILD_TESTING)
    add_executable(serial-pty-test pty_test.cc)
    target_link_libraries(serial-pty-test PRIVATE ${PROJECT_NAME} util)
    add_test(NAME serial-pty-test COMMAND serial-pty-test)
endif()
