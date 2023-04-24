varying vec4 color;
varying vec2 tex_coord;

uniform sampler2D tex;

void main() {
  gl_FragColor = texture2D(tex, tex_coord);
}
