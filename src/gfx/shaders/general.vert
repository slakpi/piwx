attribute vec3 in_pos;
attribute vec4 in_color;
attribute vec2 in_tex_coord;

uniform mat4 mvp;

varying vec4 color;
varying vec2 tex_coord;

void main() {
  gl_Position = mvp * vec4(in_pos, 1.0);
  color = in_color;
  tex_coord = in_tex_coord;
}
