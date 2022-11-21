#include "gfx.h"
#include "util.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const Point2f rectVerts[] = {{0.0f, 0.0f}, {160.0f, 0.0f}, {0.0f, 120.f}, {160.0f, 120.0f}};
static const Point2f line1Verts[] = {{0.0f, 0.0f}, {160.0f, 120.0f}};
static const Point2f line2Verts[] = {{0.0f, 120.0f}, {160.0f, 0.0f}};
static const Point2f line3Verts[] = {{240.0f, 0.0f}, {240.0f, 120.0f}};
static const Point2f line4Verts[] = {{160.0f, 60.0f}, {320.0f, 60.0f}};
static const Point2f textLoc = {0.0f, 240.0f};
static const Color4f rectColor = {1.0f, 0.0f, 0.0f, 0.5f};
static const Color4f lineColor = {0.0f, 0.0f, 1.0f, 0.5f};
static const Color4f textColor = {1.0f, 0.0f, 0.0f, 1.0f};
static const Color4f clearColor = {0.0f, 1.0f, 0.0f, 0.25f};

int main() {
  DrawResources rsrc     = NULL;
  char          msg[256] = {0};
  int           error    = 0;
  Vector2f      iconInfo = {0};
  Point2f       center   = {0};

  if (!initGraphics(&rsrc)) {
    getGfxError(rsrc, &error, msg, COUNTOF(msg));
    printf("Received error %d: %s\n", error, msg);
    cleanupGraphics(&rsrc);
    return -1;
  }

  clearSurface(rsrc, clearColor);
  drawQuad(rsrc, rectVerts, rectColor);
  drawLine(rsrc, line1Verts, lineColor, 2.0f);
  drawLine(rsrc, line2Verts, lineColor, 2.0f);
  drawLine(rsrc, line3Verts, lineColor, 2.0f);
  drawLine(rsrc, line4Verts, lineColor, 2.0f);
  drawText(rsrc, FONT_16PT, textLoc, "Hello", 5, textColor, VERT_ALIGN_CELL);

  getIconInfo(rsrc, ICON_WX_VOLCANIC_ASH, &iconInfo);
  center.coord.x = 320.0f - (iconInfo.v[0] / 2.0f);
  center.coord.y = 240.0f - (iconInfo.v[1] / 2.0f);
  drawIcon(rsrc, ICON_WX_VOLCANIC_ASH, center);

  dumpSurfaceToPng(rsrc, "test.png");
  // commitToScreen(rsrc);
  cleanupGraphics(&rsrc);
  return 0;
}
