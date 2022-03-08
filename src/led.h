/**
 * @file led.h
 */
#ifndef LED_H
#define LED_H

#include "conf_file.h"
#include "wx.h"

/**
 * @brief   Update the LEDs with new weather information.
 * @param [in] cfg PiWx configuration.
 * @param [in] wx  Circular weather station list.
 * @returns 0 if successful, non-zero otherwise.
 */
int updateLEDs(const PiwxConfig *cfg, WxStation *wx);

#endif /* LED_H */