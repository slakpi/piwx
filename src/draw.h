/**
 * @file draw.h
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
 * @param[in] commit    Commit the surface to the screen.
 */
void clearScreen(DrawResources resources, bool commit);

/**
 * @brief Draws the weather download error screen.
 * @param[in] resources The gfx context.
 * @param[in] commit    Commit the surface to the screen.
 */
void drawDownloadErrorScreen(DrawResources resources, bool commit);

/**
 * @brief Draws the downloading weather screen.
 * @param[in] resources The gfx context.
 * @param[in] commit    Commit the surface to the screen.
 */
void drawDownloadScreen(DrawResources resources, bool commit);

/**
 * @brief Draw a station's weather information.
 * @param[in] resources The gfx context.
 * @param[in] station   The weather station information.
 * @param[in] curTime   The current system time.
 * @param[in] globe     Draw the day/night globe.
 * @param[in] globePos  Eye position over the globe.
 * @param[in] commit    Commit the surface to the screen.
 */
void drawStationScreen(DrawResources resources, const WxStation *station, time_t curTime,
                       bool globe, Position globePos, bool commit);

#endif