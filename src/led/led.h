/**
 * @file led.h
 * @defmodule LedModule LED Module
 * @{
 */
#if !defined LED_H
#define LED_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @struct LEDColor
 * @brief  RGB color values for a LED.
 */
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} LEDColor;

/**
 * @brief   Initialize the LED library.
 * @details Must only be called once at program startup and must be called
 *          before @a led_setColors or @a led_finalize.
 * @param[in] dataPin    The LED string's GPIO data pin.
 * @param[in] dmaChannel The DMA channel to use for communication.
 * @param[in] maxLeds    The maximum number of LEDs expected.
 * @returns True if successful, false otherwise.
 */
bool led_init(int dataPin, int dmaChannel, size_t maxLeds);

/**
 * @brief   Set the LED colors.
 * @details If @a colors is NULL, @a count will be ignored and the entire string
 *          will be turned off.
 * @param[in] colors     The colors to assign.
 * @param[in] count      The length of the @a colors array.
 * @returns True if successful, false otherwise.
 */
bool led_setColors(const LEDColor *colors, size_t count);

/**
 * @brief Shutdown the LED library.
 */
void led_finalize();

#endif /* LED_H @} */
