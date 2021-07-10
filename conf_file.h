#ifndef CONF_FILE_H
#define CONF_FILE_H

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

#endif
