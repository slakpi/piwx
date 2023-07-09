#-------------------------------------------------------------------------------
# @brief   Converts a shader file into a string constant in a C header.
# @details Given a file name such as "vertex.shader", the function creates a C
#          header named "vertex.shader.h" with the contents of "vert.shader" in
#          a string constant named "VERTEX_SHADER_SRC". The header file is added
#          to the target's list of sources.
# @param[in] target      The target that depends on the shader.
# @param[in] shader_file The name of the shader file in this directory.
#-------------------------------------------------------------------------------
function(add_shader target shader_file)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${shader_file}.h"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader_to_header.py"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_file}"
                                  "${CMAKE_CURRENT_BINARY_DIR}/${shader_file}.h"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_file}"
    COMMENT "Creating header for ${shader_file}...")
  target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${shader_file}.h")
endfunction()

#-------------------------------------------------------------------------------
# Raspberry Pi GL libraries.
#-------------------------------------------------------------------------------
set(rpi_gl_include "/opt/vc/include")
set(rpi_gl_lib "/opt/vc/lib")

#-------------------------------------------------------------------------------
# Setup the graphics library.
#-------------------------------------------------------------------------------
add_library(gfx STATIC gfx.c draw.c img.c vec.c)
target_compile_options(gfx PRIVATE ${SIMD_C_FLAGS})
target_include_directories(gfx
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${rpi_gl_include})
target_link_directories(gfx PUBLIC ${rpi_gl_lib})
target_link_libraries(gfx PRIVATE brcmEGL brcmGLESv2 Piwx::Conf_File PNG::PNG Threads::Threads vcos)

#-------------------------------------------------------------------------------
# Make shader headers.
#-------------------------------------------------------------------------------
add_shader(gfx "alpha_tex.frag")
add_shader(gfx "general.frag")
add_shader(gfx "rgba_tex.frag")
add_shader(gfx "general.vert")

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Gfx ALIAS gfx)
