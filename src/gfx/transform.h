/**
 * @file transform.h
 */
#if !defined TRANSFORM_H
#define TRANSFORM_H

typedef float TransformMatrix[4][4];

/**
 * @brief Combine two transforms: a = a * b.
 * @param[in,out] a Accumulating transform.
 * @param[in]     b Transform to combine with @a a.
 */
void combineTransforms(TransformMatrix a, const TransformMatrix b);

/**
 * @brief Initialize an identity matrix.
 * @param[out] mat The transform to initialize.
 */
void makeIdentity(TransformMatrix mat);

/**
 * @brief Initialize a scale matrix.
 * @param[out] mat The transform to initialize.
 * @param[in]  x   The X axis scale.
 * @param[in]  y   The Y axis scale.
 * @param[in]  z   The Z axis scale.
 */
void makeScale(TransformMatrix mat, float x, float y, float z);

/**
 * @brief Initialize a translation matrix.
 * @param[out] mat The transform to initialize.
 * @param[in]  x   The X axis translation.
 * @param[in]  y   The Y axis translation.
 * @param[in]  z   The Z axis translation.
 */
void makeTranslation(TransformMatrix mat, float x, float y, float z);

/**
 * @brief Initialize an X counter clockwise rotation matrix.
 * @param[out] mat   The transform to initialize.
 * @param[in]  theta The rotation angle in radians.
 */
void makeXRotation(TransformMatrix mat, float theta);

/**
 * @brief Initialize an Y counter clockwise rotation matrix.
 * @param[out] mat   The transform to initialize.
 * @param[in]  theta The rotation angle in radians.
 */
void makeYRotation(TransformMatrix mat, float theta);

/**
 * @brief Initialize an Z counter clockwise rotation matrix.
 * @param[out] mat   The transform to initialize.
 * @param[in]  theta The rotation angle in radians.
 */
void makeZRotation(TransformMatrix mat, float theta);

#endif /* TRANSFORM_H */
