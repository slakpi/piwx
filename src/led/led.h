/**
 * @file led.h
 * @defmodule LedModule LED Module
 * @{
 */
#if !defined LED_H
#define LED_H

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
 * @brief   Set the LED colors.
 * @details If @a colors is NULL, @a count will be ignored and the entire string
 *          will be turned off.
 * @param[in] dataPin    The LED string's GPIO data pin.
 * @param[in] dmaChannel The DMA channel to use for communication.
 * @param[in] colors     The colors to assign.
 * @param[in] count      The length of the @a colors array.
 * @returns 0 if successful, non-zero otherwise.
 */
int led_setColors(int dataPin, int dmaChannel, const LEDColor *colors, size_t count);

#endif /* LED_H @} */
