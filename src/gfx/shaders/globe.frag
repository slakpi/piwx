#define M_PI         3.1415926536
#define M_PI_2       (M_PI / 2.0)
#define CIVIL        (6.0 * M_PI / 180.0)
#define NAUTICAL     (12.0 * M_PI / 180.0)
#define ASTRONOMICAL (18.0 * M_PI / 180.0)

varying vec4 color;
varying vec2 tex_coord;
varying vec3 normal;

uniform vec3 lightDir;
uniform sampler2D tex_0;  // Day map
uniform sampler2D tex_1;  // Night map
uniform sampler2D tex_2;  // Threshold
uniform sampler2D tex_3;  // Clouds

void main() {
  float angle = clamp(-acos(dot(lightDir, normal)) + M_PI_2, 0.0, ASTRONOMICAL);

  vec4 dayColor = texture2D(tex_0, tex_coord);
  vec4 nightColor = texture2D(tex_1, tex_coord);
  float alpha = texture2D(tex_2, vec2(angle / ASTRONOMICAL, 0.0)).a;
  float cloud = clamp(1.0 - alpha, 0.15, 0.5) * texture2D(tex_3, tex_coord).a;
  vec4 globe = mix(dayColor, nightColor, alpha);

  gl_FragColor = mix(globe, vec4(1.0, 1.0, 1.0, 1.0), cloud);
}
