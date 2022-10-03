/**
 * @file  gfx_prv.h
 */
#ifndef GFX_PRV_H
#define GFX_PRV_H

#include "gfx.h"
#include "vec.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FONT_ROWS 8
#define FONT_COLS 16

#define GET_EGL_ERROR(rsrc)              getEglError(rsrc, __FILE__, __LINE__)
#define GET_SHADER_ERROR(rsrc, shader)   getShaderError(rsrc, shader, __FILE__, __LINE__);
#define GET_PROGRAM_ERROR(rsrc, program) getProgramError(rsrc, program, __FILE__, __LINE__);
#define SET_ERROR(rsrc, error, msg)      setError(rsrc, error, msg, __FILE__, __LINE__);

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
  Point2f     pos;   // Vertex position
  Color4f     color; // Vertex color
  TexCoords2f tex;   // Vertex texture coordinates
} Vertex;

/**
 * @struct Texture
 * @brief  Wrapper for a GL texture.
 */
typedef struct {
  GLuint tex;           // OpenGL texture handle
  float  width, height; // Texture dimensions
} Texture;

/**
 * @struct CharacterInfo
 * @brief  Character render information.
 */
typedef struct {
  float       charWidth, charHeight;
  TexCoords2f texTopLeft;
  TexCoords2f texBottomRight;
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
typedef enum { GENERAL_VERTEX, VERTEX_COUNT } VertexShader;

/**
 * @enum  FragmentShader
 * @brief Fragment shader handles.
 */
typedef enum {
  GENERAL_FRAGMENT,
  ALPHA_TEX_FRAGMENT,
  RGBA_TEX_FRAGMENT,
  FRAGMENT_COUNT
} FragmentShader;

/**
 * @enum  Program
 * @brief Shader program handles.
 */
typedef enum { GENERAL_SHADER, ALPHA_TEX_SHADER, RGBA_TEX_SHADER, PROGRAM_COUNT } Program;

/**
 * @struct DrawResources_
 * @brief  Private implementation of the gfx DrawResources context.
 */
typedef struct {
  EGLDisplay  display;                  // EGL display object
  EGLContext  context;                  // EGL context
  EGLSurface  surface;                  // EGL surface
  int         error;                    // Last error code
  char        errorMsg[256];            // Last error message
  char        errorFile[256];           // File where the last error occurred
  long        errorLine;                // Line number of last error occurred
  int         major, minor;             // EGL API version
  GLuint      vshaders[VERTEX_COUNT];   // Vertex shaders
  GLuint      fshaders[FRAGMENT_COUNT]; // Fragment shaders
  ProgramInfo programs[PROGRAM_COUNT];  // Shader programs
  Texture     fonts[FONT_COUNT];        // Font textures
  Texture     icons[ICON_COUNT];        // Icon textures
  GLfloat     proj[4][4];               // Projection matrix
} DrawResources_;

/**
 */
bool getCharacterRenderInfo(const DrawResources_ *rsrc, Font font, char c,
                            CharacterRenderInfo *info);

/**
 * @brief Get information about the last EGL error.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void getEglError(DrawResources_ *rsrc, const char *file, long line);

/**
 * @brief Get program linker errors.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] program  Program to query.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void getProgramError(DrawResources_ *rsrc, GLuint program, const char *file, long line);

/**
 * @brief Get shader compilation errors.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] shader   Shader to query.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void getShaderError(DrawResources_ *rsrc, GLuint shader, const char *file, long line);

/**
 * @brief Reset back to the generic shader and disable all attribute arrays.
 * @param[in] rsrc The gfx context.
 */
void resetShader(const DrawResources_ *rsrc, Program program);

/**
 * @brief Set a custom error.
 * @param[in,out] rsrc gfx context to receive the error information.
 * @param[in] error    Custom error code.
 * @param[in] msg      Custom error message.
 * @param[in] file     File where the error occurred.
 * @param[in] line     Line number where the error occurred.
 */
void setError(DrawResources_ *rsrc, int error, const char *msg, const char *file, long line);

/**
 * @brief Configure a shader with the current projection matrix and a texture.
 * @param[in] rsrc The gfx context.
 * @param[in] program  The shader program to use.
 * @param[in] texture  A GL texture handle or 0 to disable textures.
 */
void setupShader(const DrawResources_ *rsrc, Program program, GLuint texture);

#endif /* GFX_PRV_H */
