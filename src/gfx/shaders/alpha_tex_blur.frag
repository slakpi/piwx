#define C0 0.37963589264252184
#define C1 0.24165830442431796
#define C2 0.06212798019854871
#define C3 0.006395769055872414

varying vec4 color;
varying vec2 tex_coord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform vec2 direction;

void main() {
  vec2 offset = (1.0 / texSize) * direction;
  vec2 offset2 = offset * 2.0;
  vec2 offset3 = offset * 3.0;
  vec4 tex_color = vec4(0.0);

  tex_color += texture2D(tex, tex_coord - offset3) * C3;
  tex_color += texture2D(tex, tex_coord - offset2) * C2;
  tex_color += texture2D(tex, tex_coord - offset) * C1;
  tex_color += texture2D(tex, tex_coord) * C0;
  tex_color += texture2D(tex, tex_coord + offset) * C1;
  tex_color += texture2D(tex, tex_coord + offset2) * C2;
  tex_color += texture2D(tex, tex_coord + offset3) * C3;

  gl_FragColor = vec4(color.rgb, tex_color.a);
}
