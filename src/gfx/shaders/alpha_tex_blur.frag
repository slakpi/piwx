// Multiplying by 1.5 gives the shadow a stronger appearance by increasing the
// weight of the blur and possibly causing the alpha to saturate.
#define C0 (0.19859610213125314 * 1.5)
#define C1 (0.17571363439579307 * 1.5)
#define C2 (0.12170274650962626 * 1.5)
#define C3 (0.06598396774984912 * 1.5)
#define C4 (0.02800156023378088 * 1.5)
#define C5 (0.00930004004532404 * 1.5)

varying vec2 tex_coord;

uniform sampler2D tex;
uniform vec2 texSize;
uniform vec2 direction;

void main() {
  vec2 offset1 = (1.0 / texSize) * direction;
  vec2 offset2 = offset1 * 2.0;
  vec2 offset3 = offset1 * 3.0;
  vec2 offset4 = offset1 * 4.0;
  vec2 offset5 = offset1 * 5.0;
  vec4 tex_color = vec4(0.0);

  tex_color += texture2D(tex, tex_coord - offset5) * C5;
  tex_color += texture2D(tex, tex_coord - offset4) * C4;
  tex_color += texture2D(tex, tex_coord - offset3) * C3;
  tex_color += texture2D(tex, tex_coord - offset2) * C2;
  tex_color += texture2D(tex, tex_coord - offset1) * C1;
  tex_color += texture2D(tex, tex_coord          ) * C0;
  tex_color += texture2D(tex, tex_coord + offset1) * C1;
  tex_color += texture2D(tex, tex_coord + offset2) * C2;
  tex_color += texture2D(tex, tex_coord + offset3) * C3;
  tex_color += texture2D(tex, tex_coord + offset4) * C4;
  tex_color += texture2D(tex, tex_coord + offset5) * C5;

  gl_FragColor = vec4(0.0, 0.0, 0.0, tex_color.a);
}
