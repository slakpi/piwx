/**
 * @file globe.c
 */
#include "conf_file.h"
#include "geo.h"
#include "gfx.h"
#include "gfx_prv.h"
#include "img.h"
#include "transform.h"
#include "util.h"
#include "vec.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define LAT_INTERVAL_DEG 10
_Static_assert(90 % LAT_INTERVAL_DEG == 0, "Invalid latitude interval");

#define LON_INTERVAL_DEG 10
_Static_assert(360 % LON_INTERVAL_DEG == 0, "Invalid longitude interval");

// The 90 degree latitude is a single point, so exclude it then divide by the
// interval, multiply by two for the two hemispheres, and add 1 for the equator.
#define LAT_COUNT (((90 - LAT_INTERVAL_DEG) / LAT_INTERVAL_DEG) * 2 + 1)

// Simply divide by the longitude circle by the interval. Add an extra longitude
// iteration to duplicate the +/- 180 longitude. This prevents a seam that would
// be created by the triangles between +170 and +180 degrees interpolating
// between a U coordinate of 0.9722 and 0. The duplicate set of vertices create
// triangles that instead interpolate a U coordinate between 0.9722 and 1.
#define LON_COUNT ((360 / LON_INTERVAL_DEG) + 1)

// Multiply the latitude divisons by the longitude divisions and add 2 for the
// poles to get the vertex count.
#define VERTEX_COUNT ((LAT_COUNT * LON_COUNT) + 2)

// The number QUAD rows is LAT_COUNT - 1 and the number of QUAD columns is
// LON_COUNT. The number of triangles around the poles is just LON_COUNT.
#define TRI_COUNT (((LAT_COUNT - 1) * LON_COUNT * 2) + (LON_COUNT * 2))

// Three indices per triangle.
//
//   NOTE: Older OpenGL ES versions only support unsigned short indices. This is
//         an issue when running on older Raspberry Pi models.
#define INDEX_COUNT (TRI_COUNT * 3)
_Static_assert(INDEX_COUNT <= USHRT_MAX, "Index count too large.");

#if defined _DEBUG
#define DRAW_AXES 1
#endif

#if defined _DEBUG
#define DUMP_GLOBE_MODEL 0
#endif

static const Position gNorthPole = {90.0, 0.0};
static const Position gSouthPole = {-90.0, 0.0};

#if DRAW_AXES == 1
static void drawAxes(const DrawResources_ *rsrc, const TransformMatrix view,
                     const TransformMatrix model, const Vector3f *lightDir);
#endif

static void drawGlobe(const DrawResources_ *rsrc, const TransformMatrix view,
                      const TransformMatrix model, const Vector3f *lightDir);

#if DUMP_GLOBE_MODEL == 1
static bool dumpGlobeModel(const DrawResources_ *rsrc, const char *imageResources);
#endif

static bool genGlobeModel(DrawResources_ *rsrc);

static void initVertex(Vertex3D *v, Position pos);

static bool loadGlobeTexture(DrawResources_ *rsrc, const char *imageResources, const char *image,
                             GLuint tex, Texture *texture);

static bool loadGlobeTextures(DrawResources_ *rsrc, const char *imageResources);

void gfx_drawGlobe(DrawResources resources, Position pos, time_t curTime,
                   const BoundingBox2D *box) {
  const DrawResources_ *rsrc = resources;

  Point2f         center;
  Vector3f        lightDir;
  Position        sunPos;
  float           width, height, scale, zoff;
  TransformMatrix view, model, tmp;

  if (!rsrc) {
    return;
  }

  if (rsrc->globeBuffers[0] == 0) {
    return;
  }

  // Calculate the subsolar point, convert it to ECEF, then make it a unit
  // direction vector and flip its direction to point back at the Earth.
  //
  //   NOTE: The Y/Z swap. See `initVertex`.
  geo_calcSubsolarPoint(&sunPos, curTime);
  geo_latLonToECEF(sunPos, &lightDir.coord.x, &lightDir.coord.z, &lightDir.coord.y);
  vectorUnit3f(&lightDir, &lightDir);
  vectorScale3f(&lightDir, &lightDir, -1.0f);

  width          = box->bottomRight.coord.x - box->topLeft.coord.x;
  height         = box->bottomRight.coord.y - box->topLeft.coord.y;
  center.coord.x = box->topLeft.coord.x + (width / 2.0f);
  center.coord.y = box->topLeft.coord.y + (height / 2.0f);

  // Scale the globe so that its semi-major diameter matches the smaller
  // dimension of the target rectangle. Set the Z offset for the view to the new
  // semi-major radius.

  if (width < height) {
    scale = (float)(width / (2.0 * GEO_WGS84_SEMI_MAJOR_M));
    zoff  = -width * 0.5f;
  } else {
    scale = (float)(height / (2.0 * GEO_WGS84_SEMI_MAJOR_M));
    zoff  = -height * 0.5f;
  }

  // The projection has the eye looking in the -Z direction with +Y pointing
  // up and +X pointing right. The viewport has +Y pointing down.
  //
  // The globe is modified ECEF using the Y axis as the polar axis instead of
  // the Z axis. The North Pole is on the +Y axis, the Prime Meridian is on the
  // +X axis, +90 degrees longitude is on the +Z axis.
  //
  // Initially, the eye is in the center of the Earth looking at the -90 degree
  // longitude and, visually, the Earth is upside down. The model transform
  // performs a 180 degree rotation on the Z axis to bring the North Pole to
  // screen up, followed by a 90 degree rotation on the Y axis to bring the
  // Prime Meridian to screen forward. The globe is then scaled down.

  makeZRotation(model, 180.0f * DEG_TO_RAD);

  makeYRotation(tmp, -90.0f * DEG_TO_RAD);
  combineTransforms(model, tmp);

  makeScale(tmp, scale, scale, scale);
  combineTransforms(model, tmp);

  // The view transform moves the scene in the -Z direction to bring the eye out
  // of the Earth. The performs X and Y rotations to the desired latitude and
  // longitude.

  makeTranslation(view, center.coord.x, center.coord.y, zoff);

  makeXRotation(tmp, -pos.lat * DEG_TO_RAD);
  combineTransforms(view, tmp);

  makeYRotation(tmp, -pos.lon * DEG_TO_RAD);
  combineTransforms(view, tmp);

  // Draw the globe.

  drawGlobe(rsrc, view, model, &lightDir);
#if DRAW_AXES == 1
  drawAxes(rsrc, view, model, &lightDir);
#endif
}

bool gfx_initGlobe(DrawResources_ *rsrc, const char *imageResources) {
  if (rsrc->globeBuffers[0] != 0) {
    return true;
  }

  if (!genGlobeModel(rsrc) || !loadGlobeTextures(rsrc, imageResources)) {
    return false;
  }

#if DUMP_GLOBE_MODEL == 1
  dumpGlobeModel(rsrc, imageResources);
#endif

  return true;
}

#if DRAW_AXES == 1
/**
 * @brief Draw the globe's axes and the subsolar point.
 * @param[in] rsrc     The gfx context.
 * @param[in] view     The view transform.
 * @param[in] model    The model transform.
 * @param[in] lightDir The light direction.
 */
static void drawAxes(const DrawResources_ *rsrc, const TransformMatrix view,
                     const TransformMatrix model, const Vector3f *lightDir) {
  GLuint   vbo;
  Vertex3D axes[] = {{{{0, 0, 0}}, gfx_Red},    {{{2 * GEO_WGS84_SEMI_MAJOR_M, 0, 0}}, gfx_Red},
                     {{{0, 0, 0}}, gfx_Green},  {{{0, 2 * GEO_WGS84_SEMI_MAJOR_M, 0}}, gfx_Green},
                     {{{0, 0, 0}}, gfx_Blue},   {{{0, 0, 2 * GEO_WGS84_SEMI_MAJOR_M}}, gfx_Blue},
                     {{{0, 0, 0}}, gfx_Yellow}, {{{0, 0, 0}}, gfx_Yellow}};

  axes[7].pos = *lightDir;
  vectorScale3f(&axes[7].pos, &axes[7].pos, -2.0f * GEO_WGS84_SEMI_MAJOR_M);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(axes), axes, GL_STATIC_DRAW);

  gfx_setup3DShader(rsrc, programGeneral3d, view, model, NULL, 0);
  glDrawArrays(GL_LINES, 0, COUNTOF(axes));
  gfx_resetShader(rsrc, programGeneral3d);

  glDeleteBuffers(1, &vbo);
}
#endif

/**
 * @brief Draw the globe.
 * @param[in] rsrc     The gfx context.
 * @param[in] view     The view transform.
 * @param[in] model    The model transform.
 * @param[in] lightDir The light direction.
 */
static void drawGlobe(const DrawResources_ *rsrc, const TransformMatrix view,
                      const TransformMatrix model, const Vector3f *lightDir) {
  GLint index = glGetUniformLocation(rsrc->programs[programGlobe].program, "lightDir");

  glBindBuffer(GL_ARRAY_BUFFER, rsrc->globeBuffers[0]);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rsrc->globeBuffers[1]);

  gfx_setup3DShader(rsrc, programGlobe, view, model, rsrc->globeTex, globeTexCount);
  glUniform3fv(index, 1, lightDir->v);
  glDrawElements(GL_TRIANGLES, INDEX_COUNT, GL_UNSIGNED_SHORT, NULL);
  gfx_resetShader(rsrc, programGlobe);
}

#if DUMP_GLOBE_MODEL == 1
/**
 * @brief   Dump the globe model to a Wavefront OBJ file.
 * @param[in] rsrc           The gfx context.
 * @param[in] imageResources The path to PiWx's image resources.
 * @returns True if successful, false otherwise.
 */
static bool dumpGlobeModel(const DrawResources_ *rsrc, const char *imageResources) {
  FILE *obj            = NULL;
  char  path[MAX_PATH] = {0};

  if (!rsrc->globe) {
    return false;
  }

  conf_getPathForImage(path, COUNTOF(path), imageResources, "globe.obj");

  obj = fopen(path, "w");

  if (!obj) {
    return false;
  }

  fprintf(obj, "o Globe\n");

  // Dump the vertices.
  for (int i = 0; i < VERTEX_COUNT; ++i) {
    fprintf(obj, "v %f %f %f\n", rsrc->globe[i].pos.coord.x, rsrc->globe[i].pos.coord.y,
            rsrc->globe[i].pos.coord.z);
  }

  // Dump the texture coordinates.
  for (int i = 0; i < VERTEX_COUNT; ++i) {
    fprintf(obj, "vt %f %f\n", rsrc->globe[i].tex.texCoord.u, rsrc->globe[i].tex.texCoord.v);
  }

  // Dump the vertex normals.
  for (int i = 0; i < VERTEX_COUNT; ++i) {
    fprintf(obj, "vn %f %f %f\n", rsrc->globe[i].normal.coord.x, rsrc->globe[i].normal.coord.y,
            rsrc->globe[i].normal.coord.z);
  }

  // Dump the triangles using vertex/texture/normal format. The OBJ file uses
  // 1-based indexing.
  for (int i = 0, tri = 0; i < TRI_COUNT; ++i, tri += 3) {
    GLuint i1 = rsrc->globeIndices[tri] + 1;
    GLuint i2 = rsrc->globeIndices[tri + 1] + 1;
    GLuint i3 = rsrc->globeIndices[tri + 2] + 1;
    fprintf(obj, "f %u/%u/%u %u/%u/%u %u/%u/%u\n", i1, i1, i1, i2, i2, i2, i3, i3, i3);
  }

  fclose(obj);

  return true;
}
#endif

/**
 * @brief   Generate the vertices and indices for the globe model.
 * @details Assumes that the globe has not already been initialized.
 * @param[in,out] rsrc The gfx context.
 * @returns True if successful, false otherwise.
 */
static bool genGlobeModel(DrawResources_ *rsrc) {
  GLushort  idx     = 0;
  int       tri     = 0;
  bool      ok      = false;
  Vertex3D *globe   = malloc(sizeof(Vertex3D) * VERTEX_COUNT);
  GLushort *indices = malloc(sizeof(GLushort) * INDEX_COUNT);
  GLuint    buffers[bufferCount];

  if (!globe || !indices) {
    goto cleanup;
  }

  glGenBuffers(bufferCount, buffers);

  if (!buffers[bufferVBO] || !buffers[bufferIBO]) {
    goto cleanup;
  }

  // Generate all vertices.

  initVertex(&globe[idx++], gNorthPole);

  for (int lat = 90 - LAT_INTERVAL_DEG; lat > -90; lat -= LAT_INTERVAL_DEG) {
    // Use <= +180 to duplicate the -180 longitude vertices with a U coordinate
    // of 1.
    for (int lon = -180; lon <= 180; lon += LON_INTERVAL_DEG) {
      Position pos = {lat, lon};
      initVertex(&globe[idx++], pos);
    }
  }

  initVertex(&globe[idx++], gSouthPole);

  // Generate the North pole triangle indices.

  for (idx = 1; idx < LON_COUNT; ++idx) {
    indices[tri++] = 0;
    indices[tri++] = idx + 1;
    indices[tri++] = idx;
  }

  indices[tri++] = 0;
  indices[tri++] = 1;
  indices[tri++] = idx++;

  // Generate the quad triangles. `lat` and `lon` are just counts for the
  // iterations. The actual degrees no longer matter.

  for (int lat = 0; lat < LAT_COUNT - 1; ++lat, ++idx) {
    for (int lon = 0; lon < LON_COUNT - 1; ++lon, ++idx) {
      indices[tri++] = idx - LON_COUNT;
      indices[tri++] = idx + 1;
      indices[tri++] = idx;

      indices[tri++] = idx - LON_COUNT;
      indices[tri++] = idx - LON_COUNT + 1;
      indices[tri++] = idx + 1;
    }

    indices[tri++] = idx - LON_COUNT;
    indices[tri++] = idx - LON_COUNT + 1;
    indices[tri++] = idx;

    indices[tri++] = idx - LON_COUNT;
    indices[tri++] = idx - LON_COUNT - LON_COUNT + 1;
    indices[tri++] = idx - LON_COUNT + 1;
  }

  // Generate the South pole triangle indices. Back up the index to the start of
  // the last latitude ring.

  idx -= LON_COUNT;

  for (; idx < VERTEX_COUNT - 2; ++idx) {
    indices[tri++] = VERTEX_COUNT - 1;
    indices[tri++] = idx;
    indices[tri++] = idx + 1;
  }

  indices[tri++] = VERTEX_COUNT - 1;
  indices[tri++] = idx;
  indices[tri++] = VERTEX_COUNT - LON_COUNT - 1;

  glBindBuffer(GL_ARRAY_BUFFER, buffers[bufferVBO]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * VERTEX_COUNT, globe, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[bufferIBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * INDEX_COUNT, indices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#if defined _DEBUG
  // Keep the CPU-side buffers in case we want to write the globe model out to
  // disk for debugging.
  rsrc->globe        = globe;
  rsrc->globeIndices = indices;
  globe              = NULL;
  indices            = NULL;
#endif

  memcpy(rsrc->globeBuffers, buffers, sizeof(buffers)); // NOLINT -- Size known.
  memset(buffers, 0, sizeof(buffers));                  // NOLINT -- Size known.

  ok = true;

cleanup:
  free(globe);
  free(indices);
  glDeleteBuffers(bufferCount, buffers);

  return ok;
}

/**
 * @brief   Initialize a vertex for a given latitude and longitude.
 * @details The Y and Z axes are swapped. ECEF uses the Z axis for the Earth's
 *          polar axis. If this convention is kept, the texture will wrap around
 *          the Z axis and then be mirrored over the XZ plane due to the
 *          projection. This will cause the texture to appear backwards.
 *
 *          If the Y axis is used as the polar axis, the texture will wrap
 *          around the Y axis, and the flip due to the projection can then be
 *          corrected by a simple rotation.
 *
 *          The other option is to keep Z as the polar axis and scale the Y axis
 *          by -1. But that effectively turns the Earth inside out making the
 *          normals inconsistent and changing the winding order of the quad
 *          triangles.
 *
 *          Yet another option is changing the projection to put +Y up in the
 *          viewport. However, that would make rendering inconsistent with
 *          framebuffer memory ordering.
 * @param[out] v   The vertex.
 * @param[in]  pos The position.
 */
static void initVertex(Vertex3D *v, Position pos) {
  geo_latLonToECEF(pos, &v->pos.coord.x, &v->pos.coord.z, &v->pos.coord.y);

  // The vertex normal is simply the direction vector of the vertex.
  vectorUnit3f(&v->normal, &v->pos);

  // (90N, 180W) => (0, 0) and (90S, 180E) => (1, 1).
  v->tex.texCoord.u = (float)((pos.lon + 180.0) / 360.0);
  v->tex.texCoord.v = (float)((-pos.lat + 90.0) / 180.0);

  // The color of the vertex does not really matter.
  v->color = gfx_White;
}

/**
 * @brief   Loads the map textures for the globe.
 * @details Assumes that the globe has not already been initialized.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     imageResources The image path.
 * @returns True if successful, false otherwise.
 */
static bool loadGlobeTextures(DrawResources_ *rsrc, const char *imageResources) {
  static const char *images[] = {"daymap.png", "nightmap.png", "threshold.png", "clouds.png"};
  _Static_assert(COUNTOF(images) == globeTexCount, "Image count must match texture count.");

  GLuint tex[globeTexCount] = {0};
  bool   ok                 = false;

  glGenTextures(globeTexCount, tex);

  for (int i = 0; i < globeTexCount; ++i) {
    if (tex[i] == 0) {
      SET_ERROR(rsrc, -1, "Failed to generate texture.");
      goto cleanup;
    }

    if (!loadGlobeTexture(rsrc, imageResources, images[i], tex[i], &rsrc->globeTex[i])) {
      goto cleanup;
    }
  }

  for (int i = 0; i < globeTexCount; ++i) {
    rsrc->globeTex[i].tex = tex[i];
    tex[i]                = 0;
  }

  ok = true;

cleanup:
  glDeleteTextures(globeTexCount, tex);

  return ok;
}

/**
 * @brief   Load a single globe texture.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     imageResources The path to PiWx's image resources.
 * @param[in]     image          Texture image file name.
 * @param[in]     tex            The GL texture handle.
 * @param[in]     texture        The texture wrapper.
 * @returns True if able to load the icon resource, false otherwise. If false,
 *          the gfx context will be updated with error information.
 */
static bool loadGlobeTexture(DrawResources_ *rsrc, const char *imageResources, const char *image,
                             GLuint tex, Texture *texture) {
  Png    png            = {0};
  char   path[MAX_PATH] = {0};
  GLenum color;
  bool   ok = false;

  conf_getPathForImage(path, COUNTOF(path), imageResources, image);

  if (!loadPng(&png, path)) {
    goto cleanup;
  }

  if (png.width < 1 || png.height < 1) {
    SET_ERROR(rsrc, -1, "Invalid map image.");
    goto cleanup;
  }

  color = gfx_pngColorToGLColor(png.color);

  if (color == GL_INVALID_ENUM) {
    SET_ERROR(rsrc, -1, "Unsupported color type for globe.");
    goto cleanup;
  }

  gfx_loadTexture(&png, tex, color, texture);

  ok = true;

cleanup:
  freePng(&png);

  return ok;
}
