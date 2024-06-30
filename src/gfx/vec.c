/**
 * @file vec.c
 * @ingroup GfxModule
 */
#include "vec.h"
#include "util.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void vectorAdd2f(Vector2f *out, const Vector2f *a, const Vector2f *b) {
  out->v[0] = a->v[0] + b->v[0];
  out->v[1] = a->v[1] + b->v[1];
}

void vectorFill2f(Vector2f *out, size_t stride, const Vector2f *in, int count) {
  uint8_t     *out_p = (uint8_t *)out->v;
  const float *in_p  = in->v;

  for (int i = 0; i < count; ++i) {
    float *out_tmp = (float *)out_p;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    out_p += stride;
  }
}

void vectorInvMagnitude2f(float *invMag, const Vector2f *vec) {
  vectorMagnitude2f(invMag, vec);

  if (*invMag < TINY_VALUE) {
    *invMag = 0.0f;
  } else {
    *invMag = 1.0f / *invMag;
  }
}

void vectorInvMagnitude3f(float *invMag, const Vector3f *vec) {
  vectorMagnitude3f(invMag, vec);

  if (*invMag < TINY_VALUE) {
    *invMag = 0.0f;
  } else {
    *invMag = 1.0f / *invMag;
  }
}

void vectorMagnitude2f(float *mag, const Vector2f *vec) {
  vectorMagnitudeSq2f(mag, vec);
  *mag = sqrtf(*mag);
}

void vectorMagnitude3f(float *mag, const Vector3f *vec) {
  vectorMagnitudeSq3f(mag, vec);
  *mag = sqrtf(*mag);
}

void vectorMagnitudeSq2f(float *magSq, const Vector2f *vec) {
  *magSq = (vec->v[0] * vec->v[0]) + (vec->v[1] * vec->v[1]);
}

void vectorMagnitudeSq3f(float *magSq, const Vector3f *vec) {
  *magSq = (vec->v[0] * vec->v[0]) + (vec->v[1] * vec->v[1]) + (vec->v[2] * vec->v[2]);
}

void vectorOrthogonal2f(Vector2f *out, const Vector2f *vec) {
  float tmp = vec->v[0];
  out->v[0] = -vec->v[1];
  out->v[1] = tmp;
}

void vectorScale2f(Vector2f *out, const Vector2f *vec, float scale) {
  out->v[0] = vec->v[0] * scale;
  out->v[1] = vec->v[1] * scale;
}

void vectorScale3f(Vector3f *out, const Vector3f *vec, float scale) {
  out->v[0] = vec->v[0] * scale;
  out->v[1] = vec->v[1] * scale;
  out->v[2] = vec->v[2] * scale;
}

void vectorSet4f(Vector4f *out, size_t stride, const Vector4f *in, int count) {
  uint8_t *out_p = (uint8_t *)out->v;

  for (int i = 0; i < count; ++i) {
    float       *out_tmp = (float *)out_p;
    const float *in_tmp  = in->v;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    out_p += stride;
  }
}

void vectorSubtract2f(Vector2f *out, const Vector2f *a, const Vector2f *b) {
  out->v[0] = a->v[0] - b->v[0];
  out->v[1] = a->v[1] - b->v[1];
}

void vectorUnit2f(Vector2f *out, const Vector2f *vec) {
  float invMag = 0.0f;

  vectorInvMagnitude2f(&invMag, vec);

  out->v[0] = vec->v[0] * invMag;
  out->v[1] = vec->v[1] * invMag;
}

void vectorUnit3f(Vector3f *out, const Vector3f *vec) {
  float invMag = 0.0f;

  vectorInvMagnitude3f(&invMag, vec);

  out->v[0] = vec->v[0] * invMag;
  out->v[1] = vec->v[1] * invMag;
  out->v[2] = vec->v[2] * invMag;
}
