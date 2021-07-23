#ifndef LED_H
#define LED_H

#include "config_helpers.h"
#include "wx.h"

/**
 * @brief   Update the LEDs with new weather information.
 * @param [in] _cfg PiWx configuration.
 * @param [in] _wx  Circular weather station list.
 * @returns 0 if successful, -1 otherwise.
 */
int updateLEDs(const PiwxConfig *_cfg, WxStation *_wx);

#endif
