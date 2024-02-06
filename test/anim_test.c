#include "anim.h"
#include "config.h"
#include "display.h"
#include "geo.h"
#include "gfx.h"
#include "util.h"
#include "wx.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const SkyCondition gKbdnSky = {.coverage = skyClear};

static const WxStation gKbdn = {.id      = "KBDN",
                                .localId = "KBDN",
                                .raw = "KBDN 030435Z AUTO 00000KT 10SM CLR 01/M00 A2978 RMK AO2",
                                .obsTime     = 1706934900,
                                .pos         = {.lat = 44.1006, .lon = -121.19799999999999},
                                .isNight     = true,
                                .wx          = wxClearNight,
                                .layers      = (SkyCondition *)&gKbdnSky,
                                .windGust    = -1,
                                .visibility  = 10,
                                .vertVis     = -1,
                                .hasTemp     = true,
                                .hasDewPoint = true,
                                .temp        = 1,
                                .dewPoint    = 0,
                                .alt         = 29.78,
                                .cat         = catVFR,
                                .blinkState  = false};

static void updateFn(Position pos, void *param);

int main() {
  const Color4f  clearColor = {{0, 0, 0, 1}};
  const Position rjtt       = {.lat = 35.5533333333, 139.7811666667};

  DrawResources resources;
  Animation     globeAnim;
  Position      globePos   = rjtt;
  char          image[256] = {0};

  if (!gfx_initGraphics(FONT_RESOURCES, IMAGE_RESOURCES, &resources)) {
    return -1;
  }

  globeAnim = makePositionAnimation(rjtt, gKbdn.pos, 30, updateFn, &globePos);

  for (int i = 0; i < 30; ++i) {
    if (!stepAnimation(globeAnim)) {
      break;
    }

    snprintf(image, COUNTOF(image), "anim%02d.png", i);

    gfx_clearSurface(resources, clearColor);
    drawGlobe(resources, gKbdn.obsTime, globePos);
    drawStation(resources, gKbdn.obsTime, &gKbdn);
    gfx_dumpSurfaceToPng(resources, image);
  }

  return 0;
}

static void updateFn(Position pos, void *param) {
  Position *outPos = param;
  *outPos          = pos;
}
