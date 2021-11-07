if (add_led_support)
  add_executable(led_test led_test.c)
  target_link_libraries(led_test ${WS2811_LIBRARIES})
endif ()
