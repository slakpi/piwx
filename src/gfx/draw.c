/**
 * @file draw.c
 */
#include "gfx.h"
#include "gfx_prv.h"
#include "util.h"
#include "vec.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_STRING_LEN 16

static void drawTriangles(const DrawResources_ *rsrc, const Vertex *vertices, size_t count,
                          Program program, GLuint texture);

static bool makeCharacter(const DrawResources_ *rsrc, Font font, char c, const Color4f *textColor,
                          const Point2f *bottomLeft, const CharInfo *info, CharVertAlign valign,
                          Vertex *vertices);

void gfx_drawIcon(DrawResources resources, Icon icon, Point2f center) {
  const DrawResources_ *rsrc   = resources;
  Vertex                buf[4] = {0};
  Color4f               color  = {{1.0f, 1.0f, 1.0f, 1.0f}};
  const Texture        *tex    = NULL;

  if (!rsrc) {
    return;
  }

  if (icon >= iconCount) {
    return;
  }

  tex = &rsrc->icons[icon];

  // Top-left
  buf[0].pos.coord.x    = floorf(center.coord.x - (tex->texSize.v[0] / 2.0f));
  buf[0].pos.coord.y    = floorf(center.coord.y - (tex->texSize.v[1] / 2.0f));
  buf[0].tex.texCoord.u = 0.0f;
  buf[0].tex.texCoord.v = 0.0f;

  // Top-right
  buf[1].pos.coord.x    = buf[0].pos.coord.x + tex->texSize.v[0];
  buf[1].pos.coord.y    = buf[0].pos.coord.y;
  buf[1].tex.texCoord.u = 1.0f;
  buf[1].tex.texCoord.v = 0.0f;

  // Bottom-left
  buf[2].pos.coord.x    = buf[0].pos.coord.x;
  buf[2].pos.coord.y    = buf[0].pos.coord.y + tex->texSize.v[1];
  buf[2].tex.texCoord.u = 0.0f;
  buf[2].tex.texCoord.v = 1.0f;

  // Bottom-right
  buf[3].pos.coord.x    = buf[1].pos.coord.x;
  buf[3].pos.coord.y    = buf[2].pos.coord.y;
  buf[3].tex.texCoord.u = 1.0f;
  buf[3].tex.texCoord.v = 1.0f;

  vectorSet4f(&buf[0].color, sizeof(Vertex), &color, COUNTOF(buf));

  drawTriangles(rsrc, buf, COUNTOF(buf), programRGBATex, tex->tex);
}

void gfx_drawLayer(DrawResources resources, Layer layer) {
  // clang-format off
  static const Vertex vertices[4] = {
    {
      {{0, 0}},
      {{1, 1, 1, 1}},
      {{0, 0}}},
    {
      {{GFX_SCREEN_WIDTH - 1, 0}},
      {{1, 1, 1, 1}},
      {{1, 0}}},
    {
      {{0, GFX_SCREEN_HEIGHT - 1}},
      {{1, 1, 1, 1}},
      {{0, 1}}},
    {
      {{GFX_SCREEN_WIDTH - 1, GFX_SCREEN_HEIGHT - 1}},
      {{1, 1, 1, 1}},
      {{1, 1}}
    }
  };
  // clang-format on

  const DrawResources_ *rsrc = resources;

  drawTriangles(rsrc, vertices, COUNTOF(vertices), programRGBATex, rsrc->layers[layer]);
}

void gfx_drawLine(DrawResources resources, const Point2f *vertices, Color4f color, float width) {
  const DrawResources_ *rsrc   = resources;
  Point2f               offset = {0};
  Vertex                buf[4] = {0};

  if (!rsrc) {
    return;
  }

  vectorSubtract2f(&offset, &vertices[1], &vertices[0]);
  vectorOrthogonal2f(&offset, &offset);
  vectorUnit2f(&offset, &offset);
  vectorScale2f(&offset, &offset, width / 2.0f);

  vectorAdd2f(&buf[0].pos, &vertices[0], &offset);
  vectorSubtract2f(&buf[1].pos, &vertices[0], &offset);
  vectorAdd2f(&buf[2].pos, &vertices[1], &offset);
  vectorSubtract2f(&buf[3].pos, &vertices[1], &offset);

  vectorSet4f(&buf[0].color, sizeof(Vertex), &color, COUNTOF(buf));

  drawTriangles(rsrc, buf, COUNTOF(buf), programGeneral, 0);
}

void gfx_drawQuad(DrawResources resources, const Point2f *vertices, Color4f color) {
  const DrawResources_ *rsrc   = resources;
  Vertex                buf[4] = {0};

  if (!rsrc) {
    return;
  }

  vectorFill2f(&buf[0].pos, sizeof(Vertex), vertices, COUNTOF(buf));
  vectorSet4f(&buf[0].color, sizeof(Vertex), &color, COUNTOF(buf));
  drawTriangles(rsrc, buf, COUNTOF(buf), programGeneral, 0);
}

void gfx_drawText(DrawResources resources, Font font, Point2f bottomLeft, const char *text,
                  size_t len, Color4f textColor, Color4f shadowColor, CharVertAlign valign) {
  const DrawResources_ *rsrc = resources;
  CharInfo              info = {0};
  Point2f               cur  = bottomLeft;
  int                   vidx = 0;
  int                   iidx = 0;
  int                   slen = len;
  GLuint                buf[3];
  Vertex                vertices[MAX_STRING_LEN * 4];
  unsigned short        indices[MAX_STRING_LEN * 6];

  if (!rsrc) {
    return;
  }

  if (!gfx_getFontInfo(resources, font, &info)) {
    return;
  }

  glGenBuffers(COUNTOF(buf), buf);

  if (!buf[bufferVBO] || !buf[bufferIBO]) {
    glDeleteBuffers(bufferCount, buf);
    return;
  }

  if (slen > MAX_STRING_LEN) {
    slen = MAX_STRING_LEN;
  }

  for (size_t i = 0; i < slen; ++i) {
    if (!makeCharacter(rsrc, font, text[i], &shadowColor, &cur, &info, valign, &vertices[vidx])) {
      continue;
    }

    indices[iidx++] = vidx;
    indices[iidx++] = vidx + 1;
    indices[iidx++] = vidx + 2;

    indices[iidx++] = vidx + 1;
    indices[iidx++] = vidx + 2;
    indices[iidx++] = vidx + 3;

    vidx += 4;

    cur.coord.x += info.cellSize.v[0];
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[bufferIBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * iidx, indices, GL_STATIC_DRAW);

  if (memcmp(&shadowColor, &gfx_Clear, sizeof(shadowColor)) != 0) {
    GLint index;

    glBindBuffer(GL_ARRAY_BUFFER, buf[bufferVBO]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vidx, vertices, GL_STATIC_DRAW);

    gfx_setupShader(rsrc, programAlphaTexBlur, rsrc->fonts[font].tex);

    index = glGetUniformLocation(rsrc->programs[programAlphaTexBlur].program, "texSize");
    glUniform2f(index, rsrc->fonts[font].texSize.v[0], rsrc->fonts[font].texSize.v[1]);

    index = glGetUniformLocation(rsrc->programs[programAlphaTexBlur].program, "direction");

    glUniform2f(index, 1.0f, 0.0f);
    glDrawElements(GL_TRIANGLES, iidx, GL_UNSIGNED_SHORT, NULL);
    glUniform2f(index, 0.0f, 1.0f);
    glDrawElements(GL_TRIANGLES, iidx, GL_UNSIGNED_SHORT, NULL);

    gfx_resetShader(rsrc, programAlphaTexBlur);

    glDeleteBuffers(1, &buf[bufferVBO]);
    glGenBuffers(1, &buf[bufferVBO]);

    if (!buf[bufferVBO]) {
      glDeleteBuffers(bufferCount, buf);
      return;
    }
  }

  vectorSet4f(&vertices[0].color, sizeof(Vertex), &textColor, vidx);

  glBindBuffer(GL_ARRAY_BUFFER, buf[bufferVBO]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vidx, vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[bufferIBO]);

  gfx_setupShader(rsrc, programAlphaTex, rsrc->fonts[font].tex);
  glDrawElements(GL_TRIANGLES, iidx, GL_UNSIGNED_SHORT, NULL);
  gfx_resetShader(rsrc, programAlphaTex);

  glDeleteBuffers(bufferCount, buf);
}

/**
 * @brief Setup a quad for drawing a character.
 * @param[in] rsrc       The gfx context.
 * @param[in] font       The font to use.
 * @param[in] c          The character to draw.
 * @param[in] textColor  The color of the character.
 * @param[in] bottomLeft The bottom-left coordinate of the character in pixels.
 * @param[out] info      The character's render information.
 * @param[out] vertices  The initialized vertices.
 */
static bool makeCharacter(const DrawResources_ *rsrc, Font font, char c, const Color4f *textColor,
                          const Point2f *bottomLeft, const CharInfo *info, CharVertAlign valign,
                          Vertex *vertices) {
  CharacterRenderInfo rndrInfo = {0};

  if (!gfx_getCharacterRenderInfo(rsrc, font, c, bottomLeft, info, valign, &rndrInfo)) {
    return false;
  }

  // Top-left
  vertices[0].pos.coord.x    = rndrInfo.bottomLeft.coord.x;
  vertices[0].pos.coord.y    = rndrInfo.bottomLeft.coord.y - info->cellSize.v[1];
  vertices[0].tex.texCoord.u = rndrInfo.texTopLeft.texCoord.u;
  vertices[0].tex.texCoord.v = rndrInfo.texTopLeft.texCoord.v;

  // Top-right
  vertices[1].pos.coord.x    = rndrInfo.bottomLeft.coord.x + info->cellSize.v[0];
  vertices[1].pos.coord.y    = vertices[0].pos.coord.y;
  vertices[1].tex.texCoord.u = rndrInfo.texBottomRight.texCoord.u;
  vertices[1].tex.texCoord.v = vertices[0].tex.texCoord.v;

  // Bottom-left
  vertices[2].pos.coord.x    = rndrInfo.bottomLeft.coord.x;
  vertices[2].pos.coord.y    = rndrInfo.bottomLeft.coord.y;
  vertices[2].tex.texCoord.u = vertices[0].tex.texCoord.u;
  vertices[2].tex.texCoord.v = rndrInfo.texBottomRight.texCoord.v;

  // Bottom-right
  vertices[3].pos.coord.x    = vertices[1].pos.coord.x;
  vertices[3].pos.coord.y    = vertices[2].pos.coord.y;
  vertices[3].tex.texCoord.u = vertices[1].tex.texCoord.u;
  vertices[3].tex.texCoord.v = vertices[2].tex.texCoord.v;

  vectorSet4f(&vertices[0].color, sizeof(Vertex), textColor, 4);

  return true;
}

/**
 * @brief Setup a temporary VBO and draw a solid triangle strip.
 * @param[in] rsrc     The gfx context.
 * @param[in] vertices An array of vertices.
 * @param[in] count    The number of vertices to draw.
 * @param[in] program  The shader program to use.
 * @param[in] texture  A GL texture handle or 0.
 */
static void drawTriangles(const DrawResources_ *rsrc, const Vertex *vertices, size_t count,
                          Program program, GLuint texture) {
  GLuint vbo = 0;

  if (count < 3) {
    return;
  }

  glGenBuffers(1, &vbo);

  if (!vbo) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * count, vertices, GL_STATIC_DRAW);

  gfx_setupShader(rsrc, program, texture);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gfx_resetShader(rsrc, program);

  glDeleteBuffers(1, &vbo);
}
