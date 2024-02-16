/**
 * @file vec.c
 */
#include "vec.h"
#include "util.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

void vectorFill2f(float *out, size_t stride, const Vector2f *in, int count) {
  uint8_t     *out_p = (uint8_t *)out;
  const float *in_p  = in->v;

  for (int i = 0; i < count; ++i) {
    float *out_tmp = (float *)out_p;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    out_p += stride;
  }
}

void vectorSet2f(float *out, size_t stride, const Vector2f *in, int count) {
  uint8_t *out_p = (uint8_t *)out;

  for (int i = 0; i < count; ++i) {
    float       *out_tmp = (float *)out_p;
    const float *in_tmp  = in->v;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    out_p += stride;
  }
}

void vectorFill3f(float *out, size_t stride, const Vector3f *in, int count) {
  uint8_t     *out_p = (uint8_t *)out;
  const float *in_p  = in->v;

  for (int i = 0; i < count; ++i) {
    float *out_tmp = (float *)out_p;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    out_p += stride;
  }
}

void vectorSet3f(float *out, size_t stride, const Vector3f *in, int count) {
  uint8_t *out_p = (uint8_t *)out;

  for (int i = 0; i < count; ++i) {
    float       *out_tmp = (float *)out_p;
    const float *in_tmp  = in->v;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    *out_tmp++           = *in_tmp++;
    out_p += stride;
  }
}

void vectorFill4f(float *out, size_t stride, const Vector4f *in, int count) {
  uint8_t     *out_p = (uint8_t *)out;
  const float *in_p  = in->v;

  for (int i = 0; i < count; ++i) {
    float *out_tmp = (float *)out_p;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    *out_tmp++     = *in_p++;
    out_p += stride;
  }
}

void vectorSet4f(float *out, size_t stride, const Vector4f *in, int count) {
  uint8_t *out_p = (uint8_t *)out;

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

void vectorAdd2f(float *out, const float *a, const float *b) {
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
}

void vectorInvMagnitude2f(float *invMag, const float *vec) {
  vectorMagnitude2f(invMag, vec);

  if (*invMag < TINY_VALUE) {
    *invMag = 0.0f;
  } else {
    *invMag = 1.0f / *invMag;
  }
}

void vectorInvMagnitude3f(float *invMag, const float *vec) {
  vectorMagnitude3f(invMag, vec);

  if (*invMag < TINY_VALUE) {
    *invMag = 0.0f;
  } else {
    *invMag = 1.0f / *invMag;
  }
}

void vectorMagnitude2f(float *mag, const float *vec) {
  *mag = sqrtf(vec[0] * vec[0] + vec[1] * vec[1]);
}

void vectorMagnitude3f(float *mag, const float *vec) {
  vectorMagnitudeSq3f(mag, vec);
  *mag = sqrtf(*mag);
}

void vectorMagnitudeSq2f(float *magSq, const float *vec) {
  *magSq = vec[0] * vec[0] + vec[1] * vec[1];
}

void vectorMagnitudeSq3f(float *magSq, const float *vec) {
  *magSq = (vec[0] * vec[0]) + (vec[1] * vec[1]) + (vec[2] * vec[2]);
}

void vectorOrthogonal2f(float *out, const float *vec) {
  float tmp = vec[0];
  out[0]    = -vec[1];
  out[1]    = tmp;
}

void vectorScale2f(float *out, const float *vec, float scale) {
  out[0] = vec[0] * scale;
  out[1] = vec[1] * scale;
}

void vectorSubtract2f(float *out, const float *a, const float *b) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
}

void vectorUnit2f(float *out, const float *vec) {
  float invMag = 0.0f;

  vectorInvMagnitude2f(&invMag, vec);

  out[0] = vec[0] * invMag;
  out[1] = vec[1] * invMag;
}

void vectorUnit3f(float *out, const float *vec) {
  float invMag = 0.0f;

  vectorInvMagnitude3f(&invMag, vec);

  out[0] = vec[0] * invMag;
  out[1] = vec[1] * invMag;
  out[2] = vec[2] * invMag;
}
