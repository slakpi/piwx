/**
 * @file conf_param.h
 */
#ifndef CONF_PARAM_H
#define CONF_PARAM_H

/**
 * @enum  ConfParam
 * @brief Lexical parameter tokens.
 */
typedef enum {
  confStations,
  confNearestAirport,
  confCycleTime,
  confHighWindSpeed,
  confHighWindBlink,
  confLED,
  confLEDBrightness,
  confLEDNightBrightness,
  confLEDDataPin,
  confLEDDMAChannel,
} ConfParam;

#endif  /* CONF_PARAM_H */
