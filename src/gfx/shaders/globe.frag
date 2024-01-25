varying vec4 color;
varying vec2 tex_coord;
varying vec3 normal;

uniform sampler2D dayTex;
uniform sampler2D nightTex;

void main() {
  gl_FragColor = texture2D(dayTex, tex_coord);
}
