/**
 * @file led.h
 */
#ifndef LED_H
#define LED_H

#include "conf_file.h"
#include "wx.h"

/**
 * @brief   Update the LEDs with new weather information.
 * @param [in] _cfg PiWx configuration.
 * @param [in] _wx  Circular weather station list.
 * @returns 0 if successful, non-zero otherwise.
 */
int updateLEDs(const PiwxConfig *_cfg, WxStation *_wx);

#endif  /* LED_H */
