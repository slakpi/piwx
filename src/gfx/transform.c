/**
 * @file transform.c
 * @ingroup GfxModule
 */
#include "transform.h"
#include <math.h>
#include <string.h>

void combineTransforms(TransformMatrix a, const TransformMatrix b) {
  TransformMatrix tmp;

  // NOTE: We don't need to do a whole lot of these matrix multiplies, so a
  //       naive implementation is fine for now. The real heavy matrix ops are
  //       going to be on the GPU anyway. But a NEON implementation would still
  //       be rad.
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      tmp[col][row] = (a[0][row] * b[col][0]) + (a[1][row] * b[col][1]) + (a[2][row] * b[col][2]) +
                      (a[3][row] * b[col][3]);
    }
  }

  memcpy(a, tmp, sizeof(tmp)); // NOLINT -- Size known.
}

void makeIdentity(TransformMatrix mat) {
  mat[0][0] = 1.0f;
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;

  mat[1][0] = 0.0f;
  mat[1][1] = 1.0f;
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;

  mat[2][0] = 0.0f;
  mat[2][1] = 0.0f;
  mat[2][2] = 1.0f;
  mat[2][3] = 0.0f;

  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}

void makeScale(TransformMatrix mat, float x, float y, float z) {
  mat[0][0] = x;
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;

  mat[1][0] = 0.0f;
  mat[1][1] = y;
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;

  mat[2][0] = 0.0f;
  mat[2][1] = 0.0f;
  mat[2][2] = z;
  mat[2][3] = 0.0f;

  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}

void makeTranslation(TransformMatrix mat, float x, float y, float z) {
  mat[0][0] = 1.0f;
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;

  mat[1][0] = 0.0f;
  mat[1][1] = 1.0f;
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;

  mat[2][0] = 0.0f;
  mat[2][1] = 0.0f;
  mat[2][2] = 1.0f;
  mat[2][3] = 0.0f;

  mat[3][0] = x;
  mat[3][1] = y;
  mat[3][2] = z;
  mat[3][3] = 1.0f;
}

void makeXRotation(TransformMatrix mat, float theta) {
  float cos_theta = cosf(theta);
  float sin_theta = sinf(theta);

  mat[0][0] = 1.0f;
  mat[0][1] = 0.0f;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;

  mat[1][0] = 0.0f;
  mat[1][1] = cos_theta;
  mat[1][2] = sin_theta;
  mat[1][3] = 0.0f;

  mat[2][0] = 0.0f;
  mat[2][1] = -sin_theta;
  mat[2][2] = cos_theta;
  mat[2][3] = 0.0f;

  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}

void makeYRotation(TransformMatrix mat, float theta) {
  float cos_theta = cosf(theta);
  float sin_theta = sinf(theta);

  mat[0][0] = cos_theta;
  mat[0][1] = 0.0f;
  mat[0][2] = -sin_theta;
  mat[0][3] = 0.0f;

  mat[1][0] = 0.0f;
  mat[1][1] = 1.0f;
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;

  mat[2][0] = sin_theta;
  mat[2][1] = 0.0f;
  mat[2][2] = cos_theta;
  mat[2][3] = 0.0f;

  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}

void makeZRotation(TransformMatrix mat, float theta) {
  float cos_theta = cosf(theta);
  float sin_theta = sinf(theta);

  mat[0][0] = cos_theta;
  mat[0][1] = sin_theta;
  mat[0][2] = 0.0f;
  mat[0][3] = 0.0f;

  mat[1][0] = -sin_theta;
  mat[1][1] = cos_theta;
  mat[1][2] = 0.0f;
  mat[1][3] = 0.0f;

  mat[2][0] = 0.0f;
  mat[2][1] = 0.0f;
  mat[2][2] = 1.0f;
  mat[2][3] = 0.0f;

  mat[3][0] = 0.0f;
  mat[3][1] = 0.0f;
  mat[3][2] = 0.0f;
  mat[3][3] = 1.0f;
}
