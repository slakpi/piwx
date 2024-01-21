/**
 * @file  gfx_prv.h
 */
#if !defined GFX_PRV_H
#define GFX_PRV_H

#include "gfx.h"
#include "img.h"
#include "vec.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FONT_ROWS 8
#define FONT_COLS 16

#define GET_EGL_ERROR(rsrc)              gfx_getEglError(rsrc, __FILE__, __LINE__)
#define GET_SHADER_ERROR(rsrc, shader)   gfx_getShaderError(rsrc, shader, __FILE__, __LINE__);
#define GET_PROGRAM_ERROR(rsrc, program) gfx_getProgramError(rsrc, program, __FILE__, __LINE__);
#define SET_ERROR(rsrc, error, msg)      gfx_setError(rsrc, error, msg, __FILE__, __LINE__);

/**
 * @typedef TexCoords2f
 * @brief   Floating-point texture coordinates.
 */
typedef Vector2f TexCoords2f;

/**
 * @struct Vertex
 * @brief  Textured, 2D vertex.
 */
typedef struct {
  Point2f     pos;
  Color4f     color;
  TexCoords2f tex;
} Vertex;

/**
 * @struct Vertex3D
 * @brief  Textured, 3D vertex.
 */
typedef struct {
  Point3f     pos;
  Color4f     color;
  TexCoords2f tex;
} Vertex3D;

/**
 * @struct Texture
 * @brief  Wrapper for a GL texture.
 */
typedef struct {
  GLuint   tex;     // OpenGL texture handle
  Vector2f texSize; // Texture dimensions in pixels
} Texture;

/**
 * @struct CharacterInfo
 * @brief  Character render information.
 */
typedef struct {
  Point2f     bottomLeft;     // The output coordinates
  Vector2f    cellSize;       // Cell size of the character in pixels
  TexCoords2f texTopLeft;     // Top-left texture coordinates in texels
  TexCoords2f texBottomRight; // Bottom-right texture coordinates in texels
} CharacterRenderInfo;

/**
 * @struct ProgramInfo
 * @brief  Shader program information.
 */
typedef struct {
  GLuint program;
  GLuint posIndex;
  GLuint colorIndex;
  GLuint texIndex;
  GLuint projIndex;
} ProgramInfo;

/**
 * @enum  VertexShader
 * @brief Vertex shader handles.
 */
typedef enum { vertexGeneral, vertexShaderCount } VertexShader;

/**
 * @enum  FragmentShader
 * @brief Fragment shader handles.
 */
typedef enum {
  fragmentGeneral,
  fragmentAlphaTex,
  fragmentRGBATex,
  fragmentShaderCount
} FragmentShader;

/**
 * @enum  Program
 * @brief Shader program handles.
 */
typedef enum { programGeneral, programAlphaTex, programRGBATex, programCount } Program;

/**
 * @enum  GlobeTexture
 * @brief Indices for the globe textures.
 */
typedef enum {
  globeDay,
  globeNight,
  globeTexCount
} GlobeTexture;

/**
 * @struct DrawResources_
 * @brief  Private implementation of the gfx DrawResources context.
 */
typedef struct {
  EGLDisplay  display;                       // EGL display object
  EGLContext  context;                       // EGL context
  EGLSurface  surface;                       // EGL surface
  int         error;                         // Last error code
  char        errorMsg[256];                 // Last error message
  char        errorFile[256];                // File where the last error occurred
  long        errorLine;                     // Line number of last error occurred
  int         major, minor;                  // EGL API version
  GLuint      vshaders[vertexShaderCount];   // Vertex shaders
  GLuint      fshaders[fragmentShaderCount]; // Fragment shaders
  ProgramInfo programs[programCount];        // Shader programs
  Texture     fonts[fontCount];              // Font textures
  Texture     icons[iconCount];              // Icon textures
  GLfloat     proj[4][4];                    // Projection matrix
  Vertex3D   *globe;                         // Globe vertices
  GLuint     *globeIndices;                  // Indices for globe triangles
  Texture     globeTex[globeTexCount];       // Globe textures
} DrawResources_;

/**
 * @brief   Calculate the rendering information for a character.
 * @details Given @a font, @a getCharacterRenderInfo computes the texture
 *          coordinates for @a c, then computes adjusted output coordinates
 *          given @a bottomLeft and @a valign.
 * @param[in] rsrc       The gfx context.
 * @param[in] font       The font to use.
 * @param[in] c          The character to render.
 * @param[in] bottomLeft The desired coordinates.
 * @param[in] valign     The vertical alignment of @a bottomLeft.
 * @param[out] rndrInfo  The character render info.
 */
bool gfx_getCharacterRenderInfo(const DrawResources_ *rsrc, Font font, char c,
                                const Point2f *bottomLeft, const CharInfo *info,
                                CharVertAlign valign, CharacterRenderInfo *rndrInfo);

/**
 * @brief Get information about the last EGL error.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void gfx_getEglError(DrawResources_ *rsrc, const char *file, long line);

/**
 * @brief Get program linker errors.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] program  Program to query.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void gfx_getProgramError(DrawResources_ *rsrc, GLuint program, const char *file, long line);

/**
 * @brief Get shader compilation errors.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] shader   Shader to query.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void gfx_getShaderError(DrawResources_ *rsrc, GLuint shader, const char *file, long line);

/**
 * @brief   Initialize the globe model for day/night display.
 * @param[in,out] rsrc           The gfx context.
 * @param[in]     imageResources The path to PiWx's image resources.
 * @returns True if successful, false otherwise.
 */
bool gfx_initGlobe(DrawResources_ *rsrc, const char *imageResources);

/**
 * @brief Configure a texture a load pixels.
 * @param[in]  png     The PNG image providing pixels.
 * @param[in]  tex     The GL texture handle.
 * @param[in]  format  The texture color format.
 * @param[out] texture The texture wrapper.
 */
void gfx_loadTexture(const Png *png, GLuint tex, GLenum format, Texture *texture);

/**
 * @brief Reset back to the generic shader and disable all attribute arrays.
 * @param[in] rsrc The gfx context.
 */
void gfx_resetShader(const DrawResources_ *rsrc, Program program);

/**
 * @brief Set a custom error.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] error    Custom error code.
 * @param[in] msg      Custom error message.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void gfx_setError(DrawResources_ *rsrc, int error, const char *msg, const char *file, long line);

/**
 * @brief Configure a shader with the current projection matrix and a texture.
 * @param[in] rsrc The gfx context.
 * @param[in] program  The shader program to use.
 * @param[in] texture  A GL texture handle or 0 to disable textures.
 */
void gfx_setupShader(const DrawResources_ *rsrc, Program program, GLuint texture);

#endif /* GFX_PRV_H */
