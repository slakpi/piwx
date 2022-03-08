if (WITH_LED_SUPPORT)
  add_executable(led_test led_test.c)
  target_link_libraries(led_test WS2811::WS2811)
endif ()
