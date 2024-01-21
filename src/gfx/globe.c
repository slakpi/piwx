/**
 * @file globe.c
 */
#include "conf_file.h"
#include "geo.h"
#include "gfx.h"
#include "gfx_prv.h"
#include "img.h"
#include "util.h"
#include "vec.h"
#include <assert.h>

#define LAT_INTERVAL_DEG 10
_Static_assert(90 % LAT_INTERVAL_DEG == 0, "Invalid latitude interval");

#define LON_INTERVAL_DEG 10
_Static_assert(360 % LON_INTERVAL_DEG == 0, "Invalid longitude interval");

// The 90 degree latitude is a single point, so exclude it then divide by the
// interval, multiply by two for the two hemispheres, and add 1 for the equator.
#define LAT_COUNT (((90 - LAT_INTERVAL_DEG) / LAT_INTERVAL_DEG) * 2 + 1)

// Simply divide by the longitude circle by the interval.
#define LON_COUNT (360 / LON_INTERVAL_DEG)

// Multiply the latitude divisons by the longitude divisions and add 2 for the
// poles to get the vertex count.
#define VERTEX_COUNT ((LAT_COUNT * LON_COUNT) + 2)

// The number QUAD rows is LAT_COUNT - 1 and the number of QUAD columns is
// LON_COUNT. The number of triangles around the poles is just LON_COUNT.
#define TRI_COUNT (((LAT_COUNT - 1) * LON_COUNT * 2) + (LON_COUNT * 2))

// Three indices per triangle.
#define INDEX_COUNT (TRI_COUNT * 3)

#if defined _DEBUG
static bool dumpGlobeModel(const DrawResources_ *rsrc, const char *imageResources);
#endif

static bool genGlobeModel(DrawResources_ *rsrc);

static void initVertex(double lat, double lon, Vertex3D *v);

static bool loadGlobeTexture(DrawResources_ *rsrc, const char *imageResources, const char *image,
                             GLuint tex, Texture *texture);

static bool loadGlobeTextures(DrawResources_ *rsrc, const char *imageResources);

void gfx_drawGlobe(DrawResources resources, double lat, double lon, double alt) {}

bool gfx_initGlobe(DrawResources_ *rsrc, const char *imageResources) {
  if (rsrc->globe) {
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

  // Dump the vertex normals. The normal is just the ECEF position vector.
  for (int i = 0; i < VERTEX_COUNT; ++i) {
    fprintf(obj, "vn %f %f %f\n", rsrc->globe[i].pos.coord.x, rsrc->globe[i].pos.coord.y,
            rsrc->globe[i].pos.coord.z);
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
  int       idx     = 0;
  int       tri     = 0;
  bool      ok      = false;
  Vertex3D *globe   = malloc(sizeof(Vertex3D) * VERTEX_COUNT);
  GLuint   *indices = malloc(sizeof(GLuint) * INDEX_COUNT);

  if (!globe || !indices) {
    goto cleanup;
  }

  // Generate all vertices.

  initVertex(90, 0, &globe[idx++]);

  for (int lat = 90 - LAT_INTERVAL_DEG; lat > -90; lat -= LAT_INTERVAL_DEG) {
    for (int lon = -180; lon < 180; lon += LON_INTERVAL_DEG) {
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
    indices[tri++] = idx;
    indices[tri++] = idx + 1;
  }

  indices[tri++] = VERTEX_COUNT - 1;
  indices[tri++] = idx;
  indices[tri++] = VERTEX_COUNT - LON_COUNT - 1;

  rsrc->globe        = globe;
  rsrc->globeIndices = indices;

  globe   = NULL;
  indices = NULL;

  ok = true;

cleanup:
  free(globe);
  free(indices);

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
  geo_LatLonToECEF(lat, lon, &v->pos.coord.x, &v->pos.coord.y, &v->pos.coord.z);
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
  static const char *images[] = {"daymap.png", "nightmap.png"};
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
  Png  png            = {0};
  char path[MAX_PATH] = {0};
  bool ok             = false;

  conf_getPathForImage(imageResources, image, path, COUNTOF(path));

  if (!loadPng(&png, path)) {
    goto cleanup;
  }

  if (png.width < 1 || png.height < 1 || png.color != PNG_COLOR_TYPE_RGB) {
    SET_ERROR(rsrc, -1, "Invalid map image.");
    goto cleanup;
  }

  gfx_loadTexture(&png, tex, GL_RGB, texture);

  ok = true;

cleanup:
  freePng(&png);

  return ok;
}
