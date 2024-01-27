#define M_PI         3.1415926536
#define M_PI_2       (M_PI / 2.0)
#define CIVIL        (6.0 * M_PI / 180.0)
#define NAUTICAL     (12.0 * M_PI / 180.0)
#define ASTRONOMICAL (18.0 * M_PI / 180.0)

varying vec4 color;
varying vec2 tex_coord;
varying vec3 normal;

uniform vec3 subsolarPoint;
uniform sampler2D tex_0;  // Day map
uniform sampler2D tex_1;  // Night map
uniform sampler2D tex_2;  // Threshold

void main() {
  vec3 lightDir = -normalize(subsolarPoint);
  float angle = clamp(-acos(dot(lightDir, normal)) + M_PI_2, 0.0, ASTRONOMICAL);

  vec4 dayColor = texture2D(tex_0, tex_coord);
  vec4 nightColor = texture2D(tex_1, tex_coord);
  vec4 alpha = texture2D(tex_2, vec2(angle / ASTRONOMICAL, 0.0));

  gl_FragColor = (alpha.a * nightColor) + ((1.0 - alpha.a) * dayColor);
}
