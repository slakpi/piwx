// Averaging filter coefficients for a 5-pixel radius. Averaging gives a
// slightly stronger appearance to the blur than a Gaussian filter.
#define C0 0.090909090909
#define C1 0.090909090909
#define C2 0.090909090909
#define C3 0.090909090909
#define C4 0.090909090909
#define C5 0.090909090909

varying vec2 tex_coord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform vec2 direction;

void main() {
  vec2 offset = (1.0 / texSize) * direction;
  vec2 offset2 = offset * 2.0;
  vec2 offset3 = offset * 3.0;
  vec2 offset4 = offset * 4.0;
  vec2 offset5 = offset * 5.0;
  vec4 tex_color = vec4(0.0);

  tex_color += texture2D(tex, tex_coord - offset5) * C5;
  tex_color += texture2D(tex, tex_coord - offset4) * C4;
  tex_color += texture2D(tex, tex_coord - offset3) * C3;
  tex_color += texture2D(tex, tex_coord - offset2) * C2;
  tex_color += texture2D(tex, tex_coord - offset) * C1;
  tex_color += texture2D(tex, tex_coord) * C0;
  tex_color += texture2D(tex, tex_coord + offset) * C1;
  tex_color += texture2D(tex, tex_coord + offset2) * C2;
  tex_color += texture2D(tex, tex_coord + offset3) * C3;
  tex_color += texture2D(tex, tex_coord + offset4) * C4;
  tex_color += texture2D(tex, tex_coord + offset5) * C5;

  gl_FragColor = vec4(0.0, 0.0, 0.0, tex_color.a);
}
