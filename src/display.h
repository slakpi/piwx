/**
 * @file display.h
 */
#if !defined DRAW_H
#define DRAW_H

#include "geo.h"
#include "gfx.h"
#include "wx.h"
#include <stdbool.h>
#include <time.h>

/**
 * @brief Clears the screen.
 * @param[in] resources The gfx context.
 */
void clearFrame(DrawResources resources);

/**
 * @brief Draws the weather download error screen.
 * @param[in] resources The gfx context.
 */
void drawDownloadError(DrawResources resources);

/**
 * @brief Draws the downloading weather screen.
 * @param[in] resources The gfx context.
 */
void drawDownloadInProgress(DrawResources resources);

/**
 * @brief Draw the day/night globe for a time and position.
 * @param[in] resources The gfx context.
 * @param[in] curTime   The current system time.
 * @param[in] pos       Eye position over the globe.
 */
void drawGlobe(DrawResources resources, time_t curTime, Position pos);

/**
 * @brief Draw a station's weather information.
 * @param[in] resources The gfx context.
 * @param[in] curTime   The current system time.
 * @param[in] station   The weather station information.
 */
void drawStation(DrawResources resources, time_t curTime, const WxStation *station);

#endif /* DISPLAY_H */
