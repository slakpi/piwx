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

#if DRAW_AXES
static void drawAxes(const DrawResources_ *rsrc, const TransformMatrix view,
                     const TransformMatrix model, const Vector3f *lightDir);
#endif

static void drawGlobe(const DrawResources_ *rsrc, const TransformMatrix view,
                      const TransformMatrix model, const Vector3f *lightDir);

#if defined _DEBUG
static bool dumpGlobeModel(const DrawResources_ *rsrc, const char *imageResources);
#endif

static bool genGlobeModel(DrawResources_ *rsrc);

static void initVertex(double lat, double lon, Vertex3D *v);

static bool loadGlobeTexture(DrawResources_ *rsrc, const char *imageResources, const char *image,
                             GLuint tex, Texture *texture);

static bool loadGlobeTextures(DrawResources_ *rsrc, const char *imageResources);

void gfx_drawGlobe(DrawResources resources, Position pos, time_t curTime,
                   const BoundingBox2D *box) {
  const DrawResources_ *rsrc = resources;

  Point2f         center;
  Vector3f        lightDir;
  double          sslat, sslon;
  float           width, height, scale, zoff;
  TransformMatrix view, model, tmp;

  if (rsrc->globeBuffers[0] == 0) {
    return;
  }

  // Calculate the subsolar point, convert it to ECEF, then make it a unit
  // direction vector and flip its direction to point back at the Earth.
  geo_calcSubsolarPoint(curTime, &sslat, &sslon);
  geo_latLonToECEF(sslat, sslon, &lightDir.v[0], &lightDir.v[1], &lightDir.v[2]);
  vectorUnit3f(&lightDir, &lightDir);
  vectorScale3f(&lightDir, &lightDir, -1.0f);

  width          = box->bottomRight.coord.x - box->topLeft.coord.x;
  height         = box->bottomRight.coord.y - box->topLeft.coord.y;
  center.coord.x = box->topLeft.coord.x + (width / 2.0f);
  center.coord.y = box->topLeft.coord.y + (height / 2.0f);

  if (width < height) {
    scale = (float)(width / (2.0 * GEO_WGS84_SEMI_MAJOR_M));
    zoff  = -width;
  } else {
    scale = (float)(height / (2.0 * GEO_WGS84_SEMI_MAJOR_M));
    zoff  = -height;
  }

  // The projection has the eye looking in the -Z direction with +Y pointing
  // down and +X pointing right. The globe is ECEF with the North pole on +Z,
  // the South pole on -Z, the Prime Meridian on +X and the antimeridian on -X.
  // Because the projection flips the Y axis, we need to negate the scale on the
  // globe's Y axis to flip the coordinates.
  //
  // Once the Y axis is flipped, the default view is for the camera to be in the
  // center of the globe looking at Antarctica with the North Pole out of the
  // screen. The East longitudes are counter clockwise up from the +X axis and
  // the West longitudes are clockwise down from the +X axis.
  //
  // The latitude is adjusted by a 90-degree clockwise rotation to bring the
  // North pole up to -Y. The negated latitude is used to rotate the globe in
  // the opposite direction of the view movement.
  //
  // The longitude is adjusted by a 90-degree clockwise rotation to bring the
  // Prime Meridian around to the eye. Because the Y axis is flipped, a positive
  // angle around the Z axis is clockwise instead of counter clockwise. Thus,
  // +90 degrees is used to adjust the Prime Meridian and the longitude is added
  // instead of subtracted. For example, to view a West longitude, the view has
  // to rotate clockwise from the Prime Meridian, which means the globe has to
  // rotate counter clockwise, so the rotation angle is negative and West
  // longitudes are negative.

  makeTranslation(view, center.coord.x, center.coord.y, zoff);

  makeXRotation(tmp, (90.0f - pos.lat) * DEG_TO_RAD);
  combineTransforms(view, tmp);

  makeZRotation(tmp, (90.0f + pos.lon) * DEG_TO_RAD);
  combineTransforms(view, tmp);

  // Flip the Y axis scale.
  makeScale(model, scale, -scale, scale);

  // After flipping the Y axis, the counter clockwise wound faces become
  // clockwise wound. Temporarily change the front face winding to clockwise.
  glFrontFace(GL_CW);

  drawGlobe(rsrc, view, model, &lightDir);
#if DRAW_AXES
  drawAxes(rsrc, view, model, &lightDir);
#endif

  // Restore the winding.
  glFrontFace(GL_CCW);
}

bool gfx_initGlobe(DrawResources_ *rsrc, const char *imageResources) {
  if (rsrc->globeBuffers[0] != 0) {
    return true;
  }

  if (!genGlobeModel(rsrc) || !loadGlobeTextures(rsrc, imageResources)) {
    return false;
  }

#if defined _DEBUG
  dumpGlobeModel(rsrc, imageResources);
#endif

  return true;
}

#if DRAW_AXES
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

#if defined _DEBUG
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

  conf_getPathForImage(imageResources, "globe.obj", path, COUNTOF(path));

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

  initVertex(90, 0, &globe[idx++]);

  for (int lat = 90 - LAT_INTERVAL_DEG; lat > -90; lat -= LAT_INTERVAL_DEG) {
    // Use <= +180 to duplicate the -180 longitude vertices with a U coordinate
    // of 1.
    for (int lon = -180; lon <= 180; lon += LON_INTERVAL_DEG) {
      initVertex(lat, lon, &globe[idx++]);
    }
  }

  initVertex(-90, 0, &globe[idx++]);

  // Generate the North pole triangle indices.

  for (idx = 1; idx < LON_COUNT; ++idx) {
    indices[tri++] = 0;
    indices[tri++] = idx;
    indices[tri++] = idx + 1;
  }

  indices[tri++] = 0;
  indices[tri++] = idx++;
  indices[tri++] = 1;

  // Generate the quad triangles. `lat` and `lon` are just counts for the
  // iterations. The actual degrees no longer matter.

  for (int lat = 0; lat < LAT_COUNT - 1; ++lat, ++idx) {
    for (int lon = 0; lon < LON_COUNT - 1; ++lon, ++idx) {
      indices[tri++] = idx - LON_COUNT;
      indices[tri++] = idx;
      indices[tri++] = idx + 1;

      indices[tri++] = idx - LON_COUNT;
      indices[tri++] = idx + 1;
      indices[tri++] = idx - LON_COUNT + 1;
    }

    indices[tri++] = idx - LON_COUNT;
    indices[tri++] = idx;
    indices[tri++] = idx - LON_COUNT + 1;

    indices[tri++] = idx - LON_COUNT;
    indices[tri++] = idx - LON_COUNT + 1;
    indices[tri++] = idx - LON_COUNT - LON_COUNT + 1;
  }

  // Generate the South pole triangle indices. Back up the index to the start of
  // the last latitude ring.

  idx -= LON_COUNT;

  for (; idx < VERTEX_COUNT - 2; ++idx) {
    indices[tri++] = VERTEX_COUNT - 1;
    indices[tri++] = idx + 1;
    indices[tri++] = idx;
  }

  indices[tri++] = VERTEX_COUNT - 1;
  indices[tri++] = VERTEX_COUNT - LON_COUNT - 1;
  indices[tri++] = idx;

  glBindBuffer(GL_ARRAY_BUFFER, buffers[bufferVBO]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3D) * VERTEX_COUNT, globe, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[bufferIBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * INDEX_COUNT, indices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#if defined _DEBUG
  rsrc->globe        = globe;
  rsrc->globeIndices = indices;
  globe              = NULL;
  indices            = NULL;
#endif

  memcpy(rsrc->globeBuffers, buffers, sizeof(buffers));
  memset(buffers, 0, sizeof(buffers));

  ok = true;

cleanup:
  free(globe);
  free(indices);
  glDeleteBuffers(bufferCount, buffers);

  return ok;
}

/**
 * @brief   Initialize a vertex for a given latitude and longitude.
 * @details Converts the coordinates to ECEF, initializes the texture
 *          coordinates assuming (90N, 180W) => (0, 0) and
 *          (90S, 180E) => (1, 1).
 * @param[in]  lat The latitude.
 * @param[in]  lon The longitude.
 * @param[out] v   The vertex.
 */
static void initVertex(double lat, double lon, Vertex3D *v) {
  geo_latLonToECEF(lat, lon, &v->pos.coord.x, &v->pos.coord.y, &v->pos.coord.z);
  vectorUnit3f(&v->normal, &v->pos);
  v->tex.texCoord.u = (float)((lon + 180.0) / 360.0);
  v->tex.texCoord.v = (float)((-lat + 90.0) / 180.0);
  v->color.color.r  = 1.0f;
  v->color.color.g  = 1.0f;
  v->color.color.b  = 1.0f;
  v->color.color.a  = 1.0f;
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

  conf_getPathForImage(imageResources, image, path, COUNTOF(path));

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
