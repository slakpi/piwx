#if !defined ANIM_H
#define ANIM_H

#include "geo.h"

/**
 * @typedef Animation
 * @brief   Abstract animation type.
 */
typedef void *Animation;

/**
 * @typedef PositionUpdateFn
 * @brief   Callback to handle updating state for a position animation.
 */
typedef void (*PositionUpdateFn)(Position pos, void *param);

/* General Animation Functions -----------------------------------------------*/

/**
 * @brief Free an animation object and its data.
 * @param[in] anim The animation object to free.
 */
void freeAnimation(Animation anim);

/**
 * @brief   Step an animation.
 * @param[in,out] anim The animation to step.
 * @returns True if the animation updated, false if it has completed.
 */
bool stepAnimation(Animation anim);

/* Position Animation Functions ----------------------------------------------*/

/**
 * @brief Create a position animation.
 * @param[in] origin   The origin position for the animation.
 * @param[in] target   The target position for the animation.
 * @param[in] step     The number duration of the animation in steps.
 * @param[in] updateFn Callback to receive position updates after a step.
 * @param[in] param    Parameter passed to @a updateFn.
 */
Animation makePositionAnimation(Position origin, Position target, unsigned int steps,
                                PositionUpdateFn updateFn, void *param);

/**
 * @brief   Reset a position animation to the beginning with new positions.
 * @details A reset retains the original step count, callback, and callback
 *          param. If these need to be updated, free the animation and re-create
 *          it with @a makePositionAnimation.
 * @param[in,out] anim   The animation to reset.
 * @param[in]     origin The new origin position.
 * @param[in]     target The new target position.
 */
void resetPositionAnimation(Animation anim, Position origin, Position target);

#endif
