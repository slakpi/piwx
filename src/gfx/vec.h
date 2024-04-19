/**
 * @file vec.h
 */
#if !defined VEC_H
#define VEC_H

#include <stddef.h>

/**
 * @union Vector2f
 * @brief 2D floating-point vector with convenience accessors.
 */
typedef union {
  struct {
    float x, y;
  } coord;
  struct {
    float u, v;
  } texCoord;
  float v[2];
} Vector2f;

/**
 * @union Vector3f
 * @brief 3D floating-point vector with convenience accessors.
 */
typedef union {
  struct {
    float x, y, z;
  } coord;
  struct {
    float r, g, b;
  } color;
  float v[3];
} Vector3f;

/**
 * @union Vector4f
 * @brief 4D floating-point vector with convenience accessors.
 */
typedef union {
  struct {
    float x, y, z, w;
  } coord;
  struct {
    float r, g, b, a;
  } color;
  float v[4];
} Vector4f;

/**
 * @brief Add two 2D vectors.
 * @param[out] out Output vector. May point to @a a or @a b.
 * @param[in]  a LHS 2D vector.
 * @param[in]  b RHS 2D vector.
 */
void vectorAdd2f(Vector2f *out, const Vector2f *a, const Vector2f *b);

/**
 * @brief   Copies 2D vectors.
 * @details The vectors in the output array may be separated by @a stride bytes.
 * @param[out] out    The output array of 2D vectors.
 * @param[in]  stride The output vector stride.
 * @param[in]  in     The input array of 2D vectors.
 * @param[in]  count  The number of 2D vectors in the input array.
 */
void vectorFill2f(Vector2f *out, size_t stride, const Vector2f *in, int count);

/**
 * @brief   Calculate the inverse magnitude of a vector; || @a vec ||^-1.
 * @details If || @a vec || < TINY_VALUE, @a invMag will be zero.
 * @param[out] invMag Inverse magnitude output.
 * @param[in]   vec   The 2D vector.
 */
void vectorInvMagnitude2f(float *invMag, const Vector2f *vec);

/**
 * @brief   Calculate the inverse magnitude of a vector; || @a vec ||^-1.
 * @details If || @a vec || < TINY_VALUE, @a invMag will be zero.
 * @param[out] invMag Inverse magnitude output.
 * @param[in]  vec    The 3D vector.
 */
void vectorInvMagnitude3f(float *invMag, const Vector3f *vec);

/**
 * @brief Calculate the magnitude of a vector; ||vec||.
 * @param[out] mag Magnitude output.
 * @param[in]  vec The 2D vector.
 */
void vectorMagnitude2f(float *mag, const Vector2f *vec);

/**
 * @brief Calculate the magnitude of a vector; ||vec||.
 * @param[out] mag Magnitude output.
 * @param[in]  vec The 3D vector.
 */
void vectorMagnitude3f(float *mag, const Vector3f *vec);

/**
 * @brief Calculate the squared magnitude of a vector; ||vec||^2.
 * @param[out] magSq Squared magnitude output.
 * @param[in]  vec   The 2D vector.
 */
void vectorMagnitudeSq2f(float *magSq, const Vector2f *vec);

/**
 * @brief Calculate the squared magnitude of a vector; ||vec||^2.
 * @param[out] magSq Squared magnitude output.
 * @param[in]  vec   The 3D vector.
 */
void vectorMagnitudeSq3f(float *magSq, const Vector3f *vec);

/**
 * @brief   Calculate a 2D vector orthogonal to the given 2D vector.
 * @details @a out = { @a vec.y, - @a vec.y }
 * @param[out] out The orthogonal vector. May point to @a vec.
 * @param[in]  vec The 2D vector.
 */
void vectorOrthogonal2f(Vector2f *out, const Vector2f *vec);

/**
 * @brief Scale a 2D vector by a scalar value.
 * @param[out] out   The scaled vector. May point to @a vec.
 * @param[in]  vec   The 2D vector to scale.
 * @param[in]  scale The scalar multiple to use.
 */
void vectorScale2f(Vector2f *out, const Vector2f *vec, float scale);

/**
 * @brief Scale a 3D vector by a scalar value.
 * @param[out] out   The scaled vector. May point to @a vec.
 * @param[in]  vec   The 2D vector to scale.
 * @param[in]  scale The scalar multiple to use.
 */
void vectorScale3f(Vector3f *out, const Vector3f *vec, float scale);

/**
 * @brief   Sets an array of 4D vectors to specified 4D vector.
 * @details The vectors in the output array may be separated by @a stride bytes.
 * @param[out] out    The output array of 4D vectors.
 * @param[in]  stride The output vector stride.
 * @param[in]  in     The input 4D vector.
 * @param[in]  count  The number of 4D vectors in the output array.
 */
void vectorSet4f(Vector4f *out, size_t stride, const Vector4f *in, int count);

/**
 * @brief Subtract two 2D vectors.
 * @param[out] out Output vector. May point to @a a or @a b.
 * @param[in]  a   LHS 2D vector.
 * @param[in]  b   RHS 2D vector.
 */
void vectorSubtract2f(Vector2f *out, const Vector2f *a, const Vector2f *b);

/**
 * @brief Calculate the unit direction of a 2D vector.
 * @param[out] unit The unit direction of @a vec. May point to @a vec.
 * @param[in]  vec  The 2D vector.
 */
void vectorUnit2f(Vector2f *unit, const Vector2f *vec);

/**
 * @brief Calculate the unit direction of a 3D vector.
 * @param[out] unit The unit direction of @a vec. May point to @a vec.
 * @param[in]  vec  The 3D vector.
 */
void vectorUnit3f(Vector3f *unit, const Vector3f *vec);

#endif /* VEC_H */
