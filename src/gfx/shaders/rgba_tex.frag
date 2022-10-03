varying vec4 color;
varying vec2 tex_coord;

uniform sampler2D tex;

void main() {
  vec4 tex_color = texture2D(tex, tex_coord);
  gl_FragColor = tex_color * color;
}
