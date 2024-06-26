/**
 * @file gfx.c
 * @ingroup GfxModule
 */
#include "conf_file.h"
#include "gfx_prv.h"
#include "gfx.h"
#include "img.h"
#include "simd.h"
#include "transform.h"
#include "util.h"
#include "vec.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <png.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// clang-format off
#include "alpha_tex.frag.h"
#include "alpha_tex_blur.frag.h"
#include "general.frag.h"
#include "globe.frag.h"
#include "rgba_tex.frag.h"
#include "general.vert.h"
#include "general3d.vert.h"
// clang-format on

#define PROJ_LEFT   0.0f
#define PROJ_RIGHT  GFX_SCREEN_WIDTH
#define PROJ_TOP    GFX_SCREEN_HEIGHT
#define PROJ_BOTTOM 0.0f
// The depth of the projection is arbitrary. For the text/icon layer, it has no
// effect on the text/icon layer. The globe will use the depth buffer to cull
// the far side of the globe.
#define PROJ_FAR    1000.0f
#define PROJ_NEAR   0.0f

/**
 * @struct FontImage
 * @brief  Font table entry.
 */
typedef struct {
  Font        font;
  const char *name;
  CharInfo    info;
} FontImage;

/**
 * @struct IconImage
 * @brief  Icon table entry.
 */
typedef struct {
  Icon        icon;
  const char *name;
} IconImage;

const Color4f gfx_Clear   = {{0.0f, 0.0f, 0.0f, 0.0f}};
const Color4f gfx_Red     = {{1.0f, 0.0f, 0.0f, 1.0f}};
const Color4f gfx_Green   = {{0.0f, 1.0f, 0.0f, 1.0f}};
const Color4f gfx_Blue    = {{0.0f, 0.0f, 1.0f, 1.0f}};
const Color4f gfx_Magenta = {{1.0f, 0.0f, 1.0f, 1.0f}};
const Color4f gfx_Yellow  = {{1.0f, 1.0f, 0.0f, 1.0f}};
const Color4f gfx_Cyan    = {{0.0f, 1.0f, 1.0f, 1.0f}};
const Color4f gfx_White   = {{1.0f, 1.0f, 1.0f, 1.0f}};
const Color4f gfx_Black   = {{0.0f, 0.0f, 0.0f, 1.0f}};

// clang-format off
static const EGLint gConfigAttribs[] = {EGL_BLUE_SIZE, 8,
                                        EGL_GREEN_SIZE, 8,
                                        EGL_RED_SIZE, 8,
                                        EGL_ALPHA_SIZE, 8,
                                        EGL_DEPTH_SIZE, 16,

                                        EGL_SAMPLE_BUFFERS, 1,
                                        EGL_SAMPLES, 4,

                                        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                        EGL_NONE};
// clang-format on

static const EGLint gContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};

static const FontImage gFontTable[] = {
    {font6pt, "sfmono6.png", {{{16.0f, 31.0f}}, 7.0f, 18.0f, 14.0f, 5.0f}},
    {font8pt, "sfmono8.png", {{{21.0f, 41.0f}}, 9.0f, 24.0f, 18.0f, 3.0f}},
    {font10pt, "sfmono10.png", {{{26.0f, 51.0f}}, 11.0f, 30.0f, 23.0f, 4.0f}},
    {font16pt, "sfmono16.png", {{{41.0f, 81.0f}}, 17.0f, 47.0f, 36.0f, 7.0f}}};

static const IconImage gIconTable[] = {{iconCatIFR, "cat_ifr.png"},
                                       {iconCatLIFR, "cat_lifr.png"},
                                       {iconCatMVFR, "cat_mvfr.png"},
                                       {iconCatUnk, "cat_unk.png"},
                                       {iconCatVFR, "cat_vfr.png"},
                                       {iconDownloadErr, "download_err.png"},
                                       {iconDownloading, "downloading.png"},
                                       {iconWxWind30, "wind_30.png"},
                                       {iconWxWind60, "wind_60.png"},
                                       {iconWxWind90, "wind_90.png"},
                                       {iconWxWind120, "wind_120.png"},
                                       {iconWxWind150, "wind_150.png"},
                                       {iconWxWind180, "wind_180.png"},
                                       {iconWxWind210, "wind_210.png"},
                                       {iconWxWind240, "wind_240.png"},
                                       {iconWxWind270, "wind_270.png"},
                                       {iconWxWind300, "wind_300.png"},
                                       {iconWxWind330, "wind_330.png"},
                                       {iconWxWind360, "wind_360.png"},
                                       {iconWxWindCalm, "wind_calm.png"},
                                       {iconWxWindUnk, "wind_unk.png"},
                                       {iconWxBrokenDay, "wx_broken_day.png"},
                                       {iconWxBrokenNight, "wx_broken_night.png"},
                                       {iconWxChanceFlurries, "wx_chance_flurries.png"},
                                       {iconWxChanceFZRA, "wx_chance_fzra.png"},
                                       {iconWxChanceRain, "wx_chance_rain.png"},
                                       {iconWxChanceSnow, "wx_chance_snow.png"},
                                       {iconWxChanceTS, "wx_chance_ts.png"},
                                       {iconWxClearDay, "wx_clear_day.png"},
                                       {iconWxClearNight, "wx_clear_night.png"},
                                       {iconWxFewDay, "wx_few_day.png"},
                                       {iconWxFewNight, "wx_few_night.png"},
                                       {iconWxFlurries, "wx_flurries.png"},
                                       {iconWxFogHaze, "wx_fog_haze.png"},
                                       {iconWxFunnelCloud, "wx_funnel_cloud.png"},
                                       {iconWxFZRA, "wx_fzra.png"},
                                       {iconWxOvercast, "wx_overcast.png"},
                                       {iconWxRain, "wx_rain.png"},
                                       {iconWxSleet, "wx_sleet.png"},
                                       {iconWxSnow, "wx_snow.png"},
                                       {iconWxThunderstorms, "wx_thunderstorms.png"},
                                       {iconWxVolcanicAsh, "wx_volcanic_ash.png"}};

static bool allocResources(DrawResources_ **rsrc);

static bool ditherPng(const Png *png, uint16_t **bmp, size_t *bytes);

static bool initEgl(DrawResources_ *rsrc);

static void initRender(DrawResources_ *rsrc);

static bool initShaders(DrawResources_ *rsrc);

static bool loadFont(DrawResources_ *rsrc, const char *fontResources, const FontImage *entry,
                     GLuint tex, Texture *texture);

static bool loadFonts(DrawResources_ *rsrc, const char *fontResources);

static bool loadIcon(DrawResources_ *rsrc, const char *imageResources, const IconImage *entry,
                     GLuint tex, Texture *texture);

static bool loadIcons(DrawResources_ *rsrc, const char *imageResources);

static void makeProjection(TransformMatrix proj);

static bool makeProgram(GLuint *program, DrawResources_ *rsrc, GLuint vert, GLuint frag);

static bool makeShader(GLuint *shader, DrawResources_ *rsrc, GLenum type, const char *source);

static bool readPixelsToPng(Png *png);

void gfx_beginLayer(DrawResources resources, Layer layer) {
  DrawResources_ *rsrc = resources;

  if (!rsrc) {
    return;
  }

  if (layer >= prvLayerCount) {
    return;
  }

  // If the stack depth is greater than zero, then the stack has already been
  // initialized and a request to begin the surface layer should be ignored. If
  // the stack depth is zero, then it has not been initialized and a request to
  // begin a layer other than the surface layer should be ignored.
  if ((rsrc->stackDepth > 0 && layer == prvLayerSurface) ||
      (rsrc->stackDepth == 0 && layer != prvLayerSurface)) {
    return;
  }

  if (rsrc->stackDepth >= MAX_FBO_NESTING) {
    return;
  }

  // Initialize the framebuffer if it is invalid.
  if (rsrc->framebuffer == 0) {
    glGenFramebuffers(1, &rsrc->framebuffer);
  }

  // Initialize the layer texture if it is invalid.
  if (rsrc->layers[layer] == 0) {
    glGenTextures(1, &rsrc->layers[layer]);
    glBindTexture(GL_TEXTURE_2D, rsrc->layers[layer]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // Initialize the layer render buffer if it is invalid.
  if (rsrc->layerBuffers[layer] == 0) {
    glGenRenderbuffers(1, &rsrc->layerBuffers[layer]);
    glBindRenderbuffer(GL_RENDERBUFFER, rsrc->layerBuffers[layer]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, GFX_SCREEN_WIDTH,
                          GFX_SCREEN_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  // Setup the complete framebuffer.
  glBindFramebuffer(GL_FRAMEBUFFER, rsrc->framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rsrc->layers[layer],
                         0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                            rsrc->layerBuffers[layer]);

  rsrc->layerStack[rsrc->stackDepth++] = layer;
}

void gfx_clearSurface(DrawResources resources, Color4f clear) {
  UNUSED(resources);

  glClearColor(clear.color.r, clear.color.g, clear.color.b, clear.color.a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

bool gfx_commitToScreen(DrawResources resources) {
  Png       png    = {0};
  uint16_t *dither = NULL;
  size_t    bytes  = 0;
  int       fb     = 0;
  bool      ok     = false;

  UNUSED(resources);

  fb = open("/dev/fb1", O_WRONLY);

  if (fb < 0) {
    return false;
  }

  if (!readPixelsToPng(&png)) {
    goto cleanup;
  }

  if (!ditherPng(&png, &dither, &bytes)) {
    goto cleanup;
  }

  if (write(fb, dither, bytes) == bytes) {
    ok = true;
  }

cleanup:
  close(fb);
  free(dither);
  freePng(&png);

  return ok;
}

void gfx_cleanupGraphics(DrawResources *resources) {
  DrawResources_ *rsrc = *resources;

  if (!rsrc) {
    return;
  }

  for (int i = 0; i < programCount; ++i) {
    glDeleteProgram(rsrc->programs[i].program);
  }

  for (int i = 0; i < fontCount; ++i) {
    glDeleteTextures(1, &rsrc->fonts[i].tex);
  }

  for (int i = 0; i < iconCount; ++i) {
    glDeleteTextures(1, &rsrc->icons[i].tex);
  }

  if (rsrc->context != EGL_NO_CONTEXT) {
    eglDestroyContext(rsrc->display, rsrc->context);
  }

  if (rsrc->display != EGL_NO_DISPLAY) {
    eglTerminate(rsrc->display);
  }

#if defined _DEBUG
  free(rsrc->globe);
  free(rsrc->globeIndices);
#endif

  glDeleteBuffers(bufferCount, rsrc->globeBuffers);

  for (int i = 0; i < globeTexCount; ++i) {
    glDeleteTextures(1, &rsrc->globeTex[i].tex);
  }

  glDeleteTextures(layerCount, rsrc->layers);
  glDeleteRenderbuffers(layerCount, rsrc->layerBuffers);
  glDeleteFramebuffers(1, &rsrc->framebuffer);

  free(rsrc);

  *resources = NULL;
}

bool gfx_dumpSurfaceToPng(DrawResources resources, const char *path) {
  Png  png = {0};
  bool ok  = false;

  UNUSED(resources);

  if (!readPixelsToPng(&png)) {
    return false;
  }

  ok = writePng(&png, path);

  freePng(&png);

  return ok;
}

void gfx_endLayer(DrawResources resources) {
  DrawResources_ *rsrc = resources;
  Layer           layer;

  if (!rsrc) {
    return;
  }

  // If the stack depth is zero, it is uninitialized and there is nothing to do.
  // If the stack depth is one, then we are down to the surface layer. The
  // surface layer should never be popped from the stack.
  if (rsrc->stackDepth < 2) {
    return;
  }

  --rsrc->stackDepth;

  layer = rsrc->layerStack[rsrc->stackDepth - 1];

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rsrc->layers[layer],
                         0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                            rsrc->layerBuffers[layer]);
}

bool gfx_getCharacterRenderInfo(const DrawResources_ *rsrc, Font font, char c,
                                const Point2f *bottomLeft, const CharInfo *info,
                                CharVertAlign valign, CharacterRenderInfo *rndrInfo) {
  int            row, col;
  const Texture *tex = NULL;

  if (!rsrc) {
    return false;
  }

  if (c < 0) {
    return false;
  }

  if (font >= fontCount) {
    return false;
  }

  row = c / FONT_COLS;
  col = c % FONT_COLS;
  tex = &rsrc->fonts[font];

  // Default to cell alignment.
  rndrInfo->bottomLeft = *bottomLeft;

  // If using baseline alignment, move the coordinates down to place the
  // character baseline at the coordinates rather than the cell edge.
  if (valign == vertAlignBaseline) {
    rndrInfo->bottomLeft.coord.y += info->baseline;
  }

  rndrInfo->cellSize                  = info->cellSize;
  rndrInfo->texTopLeft.texCoord.u     = (col * info->cellSize.v[0]) / tex->texSize.v[0];
  rndrInfo->texTopLeft.texCoord.v     = (row * info->cellSize.v[1]) / tex->texSize.v[1];
  rndrInfo->texBottomRight.texCoord.u = ((col + 1) * info->cellSize.v[0]) / tex->texSize.v[0];
  rndrInfo->texBottomRight.texCoord.v = ((row + 1) * info->cellSize.v[1]) / tex->texSize.v[1];

  return true;
}

void gfx_getEglError(DrawResources_ *rsrc, const char *file, long line) {
  const char *msg = "Unknown error.";

  strncpy_safe(rsrc->errorFile, COUNTOF(rsrc->errorFile), file);
  rsrc->errorLine = line;
  rsrc->error     = (int)eglGetError();

  switch (rsrc->error) {
  case EGL_SUCCESS:
    msg = "The last function succeeded without error.";
    break;
  case EGL_NOT_INITIALIZED:
    msg = "EGL is not initialized, or could not be initialized, for the "
          "specified EGL display connection.";
    break;
  case EGL_BAD_ACCESS:
    msg = "EGL cannot access a requested resource (for example a context "
          "is bound in another thread).";
    break;
  case EGL_BAD_ALLOC:
    msg = "EGL failed to allocate resources for the requested operation.";
    break;
  case EGL_BAD_ATTRIBUTE:
    msg = "An unrecognized attribute or attribute value was passed in the "
          "attribute list.";
    break;
  case EGL_BAD_CONTEXT:
    msg = "An EGLContext argument does not name a valid EGL rendering "
          "context.";
    break;
  case EGL_BAD_CONFIG:
    msg = "An EGLConfig argument does not name a valid EGL frame buffer "
          "configuration.";
    break;
  case EGL_BAD_CURRENT_SURFACE:
    msg = "The current surface of the calling thread is a window, pixel "
          "buffer or pixmap that is no longer valid.";
    break;
  case EGL_BAD_DISPLAY:
    msg = "An EGLDisplay argument does not name a valid EGL display "
          "connection.";
    break;
  case EGL_BAD_SURFACE:
    msg = "An EGLSurface argument does not name a valid surface (window, "
          "pixel buffer or pixmap) configured for GL rendering.";
    break;
  case EGL_BAD_MATCH:
    msg = "Arguments are inconsistent (for example, a valid context "
          "requires buffers not supplied by a valid surface).";
    break;
  case EGL_BAD_PARAMETER:
    msg = "One or more argument values are invalid.";
    break;
  case EGL_BAD_NATIVE_PIXMAP:
    msg = "A NativePixmapType argument does not refer to a valid native "
          "pixmap.";
    break;
  case EGL_BAD_NATIVE_WINDOW:
    msg = "A NativeWindowType argument does not refer to a valid native "
          "window.";
    break;
  case EGL_CONTEXT_LOST:
    msg = "A power management event has occurred. The application must "
          "destroy all contexts and reinitialise OpenGL ES state and "
          "objects to continue rendering.";
    break;
  default:
    break;
  }

  strncpy_safe(rsrc->errorMsg, COUNTOF(rsrc->errorMsg), msg);
}

bool gfx_getFontInfo(DrawResources resources, Font font, CharInfo *info) {
  UNUSED(resources);

  if (font >= fontCount) {
    return false;
  }

  *info = gFontTable[font].info;

  return true;
}

bool gfx_getIconInfo(DrawResources resources, Icon icon, Vector2f *size) {
  DrawResources_ *rsrc = resources;

  if (!rsrc) {
    return false;
  }

  if (icon >= iconCount) {
    return false;
  }

  *size = rsrc->icons[icon].texSize;

  return true;
}

void gfx_getGfxError(DrawResources resources, int *error, char *msg, size_t len) {
  DrawResources_ *rsrc = resources;

  if (!rsrc) {
    return;
  }

  *error = (int)rsrc->error;

  if (msg) {
    size_t l = min(COUNTOF(rsrc->errorMsg), len);
    strncpy_safe(msg, l, rsrc->errorMsg);
  }
}

void gfx_getProgramError(DrawResources_ *rsrc, GLuint program, const char *file, long line) {
  GLsizei len = 0;

  glGetProgramInfoLog(program, COUNTOF(rsrc->errorMsg), &len, rsrc->errorMsg);
  rsrc->error     = -1;
  rsrc->errorLine = line;
  strncpy_safe(rsrc->errorFile, COUNTOF(rsrc->errorFile), file);
}

void gfx_getShaderError(DrawResources_ *rsrc, GLuint shader, const char *file, long line) {
  GLsizei len = 0;

  glGetShaderInfoLog(shader, COUNTOF(rsrc->errorMsg), &len, rsrc->errorMsg);
  rsrc->error     = -1;
  rsrc->errorLine = line;
  strncpy_safe(rsrc->errorFile, COUNTOF(rsrc->errorFile), file);
}

bool gfx_initGraphics(const char *fontResources, const char *imageResources,
                      DrawResources *resources) {
  DrawResources_ *rsrc = NULL;
  bool            ok   = false;

  if (!resources) {
    return false;
  }

  *resources = NULL;

  if (!fontResources || !imageResources) {
    return false;
  }

  if (!allocResources(&rsrc)) {
    return false;
  }

  if (!initEgl(rsrc)) {
    goto cleanup;
  }

  if (!initShaders(rsrc)) {
    goto cleanup;
  }

  if (!loadFonts(rsrc, fontResources)) {
    goto cleanup;
  }

  if (!loadIcons(rsrc, imageResources)) {
    goto cleanup;
  }

  if (!gfx_initGlobe(rsrc, imageResources)) {
    goto cleanup;
  }

  initRender(rsrc);

  *resources = rsrc;
  rsrc       = NULL;
  ok         = true;

cleanup:
  gfx_cleanupGraphics((DrawResources *)&rsrc);

  return ok;
}

void gfx_loadTexture(const Png *png, GLuint tex, GLenum format, Texture *texture) {
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, format, png->width, png->height, 0, format, GL_UNSIGNED_BYTE,
               png->rows[0]);

  texture->texSize.v[0] = png->width;
  texture->texSize.v[1] = png->height;
}

GLenum gfx_pngColorToGLColor(png_byte color) {
  switch (color) {
  case PNG_COLOR_TYPE_RGB:
    return GL_RGB;
  case PNG_COLOR_TYPE_RGBA:
    return GL_RGBA;
  case PNG_COLOR_TYPE_GRAY:
    return GL_ALPHA;
  default:
    return GL_INVALID_ENUM;
  }
}

void gfx_resetShader(const DrawResources_ *rsrc, Program program) {
  const ProgramInfo *prg = &rsrc->programs[program];

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(prg->posIndex);
  glDisableVertexAttribArray(prg->colorIndex);
  glDisableVertexAttribArray(prg->texIndex);

  glUseProgram(rsrc->programs[programGeneral].program);
}

void gfx_setError(DrawResources_ *rsrc, int error, const char *msg, const char *file, long line) {
  strncpy_safe(rsrc->errorMsg, COUNTOF(rsrc->errorMsg), msg);
  rsrc->error     = error;
  rsrc->errorLine = line;
  strncpy_safe(rsrc->errorFile, COUNTOF(rsrc->errorFile), file);
}

void gfx_setupShader(const DrawResources_ *rsrc, Program program, GLuint texture) {
  const ProgramInfo *prg = &rsrc->programs[program];

  GLint samplerIndex;

  if (!glIsProgram(prg->program)) {
    return;
  }

  glUseProgram(prg->program);
  glUniformMatrix4fv(prg->mvpIndex, 1, GL_FALSE, (const GLfloat *)rsrc->proj);

  glEnableVertexAttribArray(prg->posIndex);
  glVertexAttribPointer(prg->posIndex, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (const void *)offsetof(Vertex, pos));

  glEnableVertexAttribArray(prg->colorIndex);
  glVertexAttribPointer(prg->colorIndex, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (const void *)offsetof(Vertex, color));

  glEnableVertexAttribArray(prg->texIndex);
  glVertexAttribPointer(prg->texIndex, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (const void *)offsetof(Vertex, tex));

  samplerIndex = glGetUniformLocation(prg->program, "tex");
  glUniform1i(samplerIndex, 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
}

void gfx_setup3DShader(const DrawResources_ *rsrc, Program program, const TransformMatrix view,
                       const TransformMatrix model, const Texture *textures,
                       unsigned int textureCount) {
  const ProgramInfo *prg = &rsrc->programs[program];

  TransformMatrix mvp;

  memcpy(mvp, rsrc->proj, sizeof(rsrc->proj)); // NOLINT -- Size known.
  combineTransforms(mvp, view);
  combineTransforms(mvp, model);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glUseProgram(prg->program);
  glUniformMatrix4fv(prg->mvpIndex, 1, GL_FALSE, (const GLfloat *)mvp);

  glEnableVertexAttribArray(prg->posIndex);
  glVertexAttribPointer(prg->posIndex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                        (const void *)offsetof(Vertex3D, pos));

  glEnableVertexAttribArray(prg->colorIndex);
  glVertexAttribPointer(prg->colorIndex, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                        (const void *)offsetof(Vertex3D, color));

  glEnableVertexAttribArray(prg->texIndex);
  glVertexAttribPointer(prg->texIndex, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                        (const void *)offsetof(Vertex3D, tex));

  glEnableVertexAttribArray(prg->normalIndex);
  glVertexAttribPointer(prg->normalIndex, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                        (const void *)offsetof(Vertex3D, normal));

  textureCount = umin(textureCount, MAX_TEXTURES);

  for (unsigned int i = 0; i < textureCount; ++i) {
    char  samplerName[16] = {0};
    GLint samplerIndex;

    // NOLINTNEXTLINE -- snprintf is sufficient; buffer size known.
    snprintf(samplerName, COUNTOF(samplerName), "tex_%u", i);
    samplerIndex = glGetUniformLocation(prg->program, samplerName);
    glUniform1i(samplerIndex, i);

    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures[i].tex);
  }
}

/**
 * @brief   Allocates and initializes a new gfx context.
 * @param[out] rsrc Pointer to the new gfx context.
 * @returns True if able to allocate the gfx context, false otherwise.
 */
static bool allocResources(DrawResources_ **rsrc) {
  if (!rsrc) {
    return false;
  }

  *rsrc = malloc(sizeof(DrawResources_));

  if (!*rsrc) {
    return false;
  }

  memset(*rsrc, 0, sizeof(**rsrc)); // NOLINT -- Size known.
  (*rsrc)->display = EGL_NO_DISPLAY;
  (*rsrc)->context = EGL_NO_CONTEXT;

  return true;
}

/**
 * @brief   Initialize EGL.
 * @param[in,out] rsrc The gfx context.
 * @returns True if able to initialize EGL, false otherwise. If false, the gfx
 *          context will be updated with error information.
 */
static bool initEgl(DrawResources_ *rsrc) {
  EGLint    numConfigs = 0;
  EGLConfig config     = {0};

  rsrc->display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL);

  if (rsrc->display == EGL_NO_DISPLAY) {
    GET_EGL_ERROR(rsrc);
    return false;
  }

  if (eglInitialize(rsrc->display, &rsrc->major, &rsrc->minor) != EGL_TRUE) {
    GET_EGL_ERROR(rsrc);
    return false;
  }

  if (eglChooseConfig(rsrc->display, gConfigAttribs, &config, 1, &numConfigs) != EGL_TRUE) {
    GET_EGL_ERROR(rsrc);
    return false;
  }

  eglBindAPI(EGL_OPENGL_API);

  rsrc->context = eglCreateContext(rsrc->display, config, EGL_NO_CONTEXT, gContextAttribs);

  if (rsrc->context == EGL_NO_CONTEXT) {
    GET_EGL_ERROR(rsrc);
    return false;
  }

  eglMakeCurrent(rsrc->display, EGL_NO_SURFACE, EGL_NO_SURFACE, rsrc->context);

  return true;
}

/**
 * @brief   Initialize shaders for the gfx context.
 * @param[in] rsrc The gfx context.
 * @returns True if able to initialize all shaders, false otherwise.
 */
static bool initShaders(DrawResources_ *rsrc) {
  typedef struct {
    GLuint v, f;
  } Link;

  static const char *vsrc[] = {GENERAL_VERT_SRC, GENERAL3D_VERT_SRC};
  _Static_assert(COUNTOF(vsrc) == vertexShaderCount, "Vertex table missing shader(s).");

  static const char *fsrc[] = {GENERAL_FRAG_SRC, ALPHA_TEX_FRAG_SRC, ALPHA_TEX_BLUR_FRAG_SRC,
                               RGBA_TEX_FRAG_SRC, GLOBE_FRAG_SRC};
  _Static_assert(COUNTOF(fsrc) == fragmentShaderCount, "Fragment table missing shader(s).");

  static const Link linkTable[] = {
      {vertexGeneral, fragmentGeneral},  {vertexGeneral3d, fragmentGeneral},
      {vertexGeneral, fragmentAlphaTex}, {vertexGeneral, fragmentAlphaTexBlur},
      {vertexGeneral, fragmentRGBATex},  {vertexGeneral3d, fragmentGlobe}};
  _Static_assert(COUNTOF(linkTable) == programCount, "Link table length must match program count.");

  GLuint vshaders[vertexShaderCount]   = {0};
  GLuint fshaders[fragmentShaderCount] = {0};
  bool   ok                            = false;

  for (int i = 0; i < vertexShaderCount; ++i) {
    if (!makeShader(&vshaders[i], rsrc, GL_VERTEX_SHADER, vsrc[i])) {
      goto cleanup;
    }
  }

  for (int i = 0; i < fragmentShaderCount; ++i) {
    if (!makeShader(&fshaders[i], rsrc, GL_FRAGMENT_SHADER, fsrc[i])) {
      goto cleanup;
    }
  }

  for (int i = 0; i < programCount; ++i) {
    ProgramInfo *prg = &rsrc->programs[i];

    GLuint vert = vshaders[linkTable[i].v];
    GLuint frag = fshaders[linkTable[i].f];

    if (!makeProgram(&prg->program, rsrc, vert, frag)) {
      goto cleanup;
    }

    prg->posIndex    = glGetAttribLocation(prg->program, "in_pos");
    prg->colorIndex  = glGetAttribLocation(prg->program, "in_color");
    prg->texIndex    = glGetAttribLocation(prg->program, "in_tex_coord");
    prg->normalIndex = glGetAttribLocation(prg->program, "in_normal");
    prg->mvpIndex    = glGetUniformLocation(prg->program, "mvp");
  }

  ok = true;

cleanup:
  for (int i = 0; i < vertexShaderCount; ++i) {
    glDeleteShader(vshaders[i]);
  }

  for (int i = 0; i < fragmentShaderCount; ++i) {
    glDeleteShader(fshaders[i]);
  }

  return ok;
}

/**
 * @brief   Compile a shader.
 * @param[out]    shader New shader.
 * @param[in,out] rsrc   The gfx context.
 * @param[in]     type   The type of shader to compile.
 * @param[in]     source The shader source code.
 * @returns True if able to compile the shader, false otherwise. If false, the
 *          gfx context will be updated with error information.
 */
static bool makeShader(GLuint *shader, DrawResources_ *rsrc, GLenum type, const char *source) {
  GLint stat = 0;

  *shader = glCreateShader(type);

  if (*shader == 0) {
    SET_ERROR(rsrc, -1, "Failed to create shader.");
    return false;
  }

  glShaderSource(*shader, 1, &source, NULL);
  glCompileShader(*shader);
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &stat);

  if (stat != GL_TRUE) {
    GET_SHADER_ERROR(rsrc, *shader);
    return false;
  }

  return true;
}

/**
 * @brief   Link a shader program.
 * @param[out]    shader New program.
 * @param[in,out] rsrc   The gfx context.
 * @param[in]     vert   The vertex shader to use.
 * @param[in]     frag   The fragment shader to use.
 * @returns True if able to link the program false otherwise. If false, the gfx
 *          context will be updated with error information.
 */
static bool makeProgram(GLuint *program, DrawResources_ *rsrc, GLuint vert, GLuint frag) {
  GLint stat = 0;

  *program = glCreateProgram();

  if (*program == 0) {
    SET_ERROR(rsrc, -1, "Failed to create program.");
    return false;
  }

  glAttachShader(*program, vert);
  glAttachShader(*program, frag);
  glLinkProgram(*program);
  glGetProgramiv(*program, GL_LINK_STATUS, &stat);

  if (stat != GL_TRUE) {
    GET_PROGRAM_ERROR(rsrc, *program);
    return false;
  }

  return true;
}

/**
 * @brief   Load font textures.
 * @details A font image must be 16 characters by 8 characters. The image must
 *          also be an 8-bit grayscale image.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     fontResources  The path to PiWx's font resources.
 * @returns True if able to load all font resources, false otherwise. If false,
 *          the gfx context will be updated with error information.
 */
static bool loadFonts(DrawResources_ *rsrc, const char *fontResources) {
  GLuint tex[fontCount] = {0};
  bool   ok             = false;

  glGenTextures(fontCount, tex);

  // Load all of the fonts in the table.
  for (int i = 0; i < fontCount; ++i) {
    if (tex[i] == 0) {
      SET_ERROR(rsrc, -1, "Failed to generate texture.");
      goto cleanup;
    }

    if (!loadFont(rsrc, fontResources, &gFontTable[i], tex[i], &rsrc->fonts[i])) {
      goto cleanup;
    }
  }

  // If we load all of the fonts, copy the GL texture handles over.
  for (int i = 0; i < fontCount; ++i) {
    rsrc->fonts[i].tex = tex[i];
    tex[i]             = 0;
  }

  ok = true;

cleanup:
  glDeleteTextures(fontCount, tex);

  return ok;
}

/**
 * @brief Load a single font image.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     fontResources  The path to PiWx's font resources.
 * @param[in]     entry          Font table entry.
 * @param[in]     tex            The GL texture handle.
 * @param[in]     texture        The texture wrapper.
 * @returns True if able to load the font resource, false otherwise. If false,
 *          the gfx context will be updated with error information.
 */
static bool loadFont(DrawResources_ *rsrc, const char *fontResources, const FontImage *entry,
                     GLuint tex, Texture *texture) {
  Png  png            = {0};
  char path[MAX_PATH] = {0};
  bool ok             = false;

  // Get the fully-qualified path to the font image.
  conf_getPathForFont(path, COUNTOF(path), fontResources, entry->name);

  if (!loadPng(&png, path)) {
    SET_ERROR(rsrc, -1, path);
    goto cleanup;
  }

  // Validate the image dimensions and color type. Don't worry about mismatching
  // with the character info.
  if (png.width % FONT_COLS != 0 || png.height % FONT_ROWS != 0 || png.bits != 8 ||
      png.color != PNG_COLOR_TYPE_GRAY) {
    SET_ERROR(rsrc, -1, "Invalid font image.");
    goto cleanup;
  }

  gfx_loadTexture(&png, tex, GL_ALPHA, texture);

  ok = true;

cleanup:
  freePng(&png);

  return ok;
}

/**
 * @brief   Load icon textures.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     imageResources The path to PiWx's image resources.
 * @returns True if able to load all icon resources, false otherwise. If false,
 *          the gfx context will be updated with error information.
 */
static bool loadIcons(DrawResources_ *rsrc, const char *imageResources) {
  GLuint tex[iconCount] = {0};
  bool   ok             = false;

  glGenTextures(iconCount, tex);

  // Load all of the icons in the table.
  for (int i = 0; i < iconCount; ++i) {
    if (tex[i] == 0) {
      SET_ERROR(rsrc, -1, "Failed to generate texture.");
      goto cleanup;
    }

    if (!loadIcon(rsrc, imageResources, &gIconTable[i], tex[i], &rsrc->icons[i])) {
      goto cleanup;
    }
  }

  // If we load all of the icons, copy the GL texture handles over.
  for (int i = 0; i < iconCount; ++i) {
    rsrc->icons[i].tex = tex[i];
    tex[i]             = 0;
  }

  ok = true;

cleanup:
  glDeleteTextures(iconCount, tex);

  return ok;
}

/**
 * @brief Load a single icon image.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     imageResources The path to PiWx's image resources.
 * @param[in]     entry          Icon table entry.
 * @param[in]     tex            The GL texture handle.
 * @param[in]     texture        The texture wrapper.
 * @returns True if able to load the icon resource, false otherwise. If false,
 *          the gfx context will be updated with error information.
 */
static bool loadIcon(DrawResources_ *rsrc, const char *imageResources, const IconImage *entry,
                     GLuint tex, Texture *texture) {
  Png  png            = {0};
  char path[MAX_PATH] = {0};
  bool ok             = false;

  // Get the fully-qualified path to the icon image.
  conf_getPathForImage(path, COUNTOF(path), imageResources, entry->name);

  if (!loadPng(&png, path)) {
    SET_ERROR(rsrc, -1, path);
    goto cleanup;
  }

  // Validate the image dimensions and color type.
  if (png.width < 1 || png.height < 1 || png.color != PNG_COLOR_TYPE_RGBA) {
    SET_ERROR(rsrc, -1, "Invalid icon image.");
    goto cleanup;
  }

  gfx_loadTexture(&png, tex, GL_RGBA, texture);

  ok = true;

cleanup:
  freePng(&png);

  return ok;
}

/**
 * @brief Initialize OpenGL for rendering.
 * @param[in] rsrc The gfx context.
 */
static void initRender(DrawResources_ *rsrc) {
  gfx_beginLayer(rsrc, prvLayerSurface);

  glViewport(0, 0, (GLsizei)GFX_SCREEN_WIDTH, (GLsizei)GFX_SCREEN_HEIGHT);

  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  makeProjection(rsrc->proj);
  gfx_resetShader(rsrc, programGeneral);
}

/**
 * @brief Initialize the projection with a column-major orthographic matrix.
 * @param[in,out] proj The projection matrix.
 */
static void makeProjection(TransformMatrix proj) {
  proj[0][0] = 2.0f / (PROJ_RIGHT - PROJ_LEFT - 1);
  proj[0][1] = 0.0f;
  proj[0][2] = 0.0f;
  proj[0][3] = 0.0f;

  proj[1][0] = 0.0f;
  proj[1][1] = 2.0f / (PROJ_TOP - PROJ_BOTTOM - 1);
  proj[1][2] = 0.0f;
  proj[1][3] = 0.0f;

  proj[2][0] = 0.0f;
  proj[2][1] = 0.0f;
  proj[2][2] = -2.0f / (PROJ_FAR - PROJ_NEAR);
  proj[2][3] = 0.0f;

  proj[3][0] = -(PROJ_RIGHT + PROJ_LEFT) / (PROJ_RIGHT - PROJ_LEFT);
  proj[3][1] = -(PROJ_TOP + PROJ_BOTTOM) / (PROJ_TOP - PROJ_BOTTOM);
  proj[3][2] = -(PROJ_FAR + PROJ_NEAR) / (PROJ_FAR - PROJ_NEAR);
  proj[3][3] = 1.0f;
}

/**
 * @brief   Read pixels from OpenGL into a PNG.
 * @param[out] png The PNG created from the OpenGL pixel data.
 * @returns True if able to allocate memory for a PNG, false otherwise.
 */
static bool readPixelsToPng(Png *png) {
  if (!allocPng(png, 8, PNG_COLOR_TYPE_RGBA, GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, 4)) {
    return false;
  }

  // The only useful pair OpenGL ES supports is GL_RGBA
  glReadPixels(0, 0, GFX_SCREEN_WIDTH, GFX_SCREEN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, png->rows[0]);

  return true;
}

/**
 * @brief   Convert a RGBA8888 PNG to a RGB565 bitmap.
 * @param[in]  png   The PNG to dither.
 * @param[out] bmp   The 16-bit RGB565 bitmap.
 * @param[out] bytes The size of the bitmap in bytes.
 * @returns True if able to convert the PNG, false if the PNG is invalid or
 *          unable to allocate memory for the bitmap.
 */
bool ditherPng(const Png *png, uint16_t **bmp, size_t *bytes) {
  uint16_t *q  = NULL;
  uint8_t  *p  = NULL;
  size_t    px = 0;

  if (!png || !bmp) {
    return false;
  }

  px = png->width * png->height;

  if (px < 1) {
    return false;
  }

  *bytes = sizeof(uint16_t) * px;
  *bmp   = aligned_alloc(sizeof(uint16_t), *bytes);

  if (!*bmp) {
    return false;
  }

  p = png->rows[0];
  q = *bmp;

  for (size_t i = 0; i < px; ++i) {
    *q++ = ditherPixel(p);
    p += 4;
  }

  return true;
}
