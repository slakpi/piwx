/**
 * @file conf_file_prv.h
 */
#if !defined CONF_FILE_PRV_H
#define CONF_FILE_PRV_H

#include "conf_file.h"
#include "geo.h"
#include "log.h"
#include <stdbool.h>
#include <stdio.h>

#define DEFAULT_CYCLE_TIME           60
#define DEFAULT_HIGH_WIND_SPEED      25
#define DEFAULT_HIGH_WIND_BLINK      false
#define DEFAULT_LED_BRIGHTNESS       32
#define DEFAULT_LED_NIGHT_BRIGHTNESS 32
#define DEFAULT_LED_DATA_PIN         18
#define DEFAULT_LED_DMA_CHANNEL      10
#define DEFAULT_LOG_LEVEL            logWarning
#define DEFAULT_DAYLIGHT             daylightCivil
#define DEFAULT_DRAW_GLOBE           true

/**
 * @brief   Parse configuration settings from a file stream.
 * @param[in,out] cfg     The configuration struct to receive the settings.
 * @param[in]     cfgFile The file stream to parse.
 * @returns True if the parse succeeds, otherwise false.
 */
bool conf_parseStream(PiwxConfig *cfg, FILE *cfgFile);

#endif /* CONF_FILE_PRV_H */
