/**
 * @file anim.c
 * @ingroup Piwx
 */
#include "anim.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @typedef AnimationData
 * @brief   Opaque type for animation-specific data.
 */
typedef void *AnimationData;

/**
 * @typedef AnimationStepFn
 * @brief   Callback to handle the animation-specific step operation.
 */
typedef void (*AnimationStepFn)(unsigned int step, AnimationData data);

/**
 * @typedef AnimationCleanFn
 * @brief   Callback to handle animation-specific data cleanup.
 */
typedef void (*AnimationCleanFn)(AnimationData data);

/**
 * @struct Animation_
 * @brief  The concrete definition of @a Animation.
 */
typedef struct {
  AnimationStepFn  stepFn;
  AnimationCleanFn cleanFn;
  AnimationData    data;
  unsigned int     steps;
  unsigned int     curStep;
  bool             loop;
} Animation_;

/**
 * @struct PositionAnimationData
 * @brief  Data specific to a position animation.
 */
typedef struct {
  Position         origin;
  Position         delta;
  PositionUpdateFn updateFn;
  void            *updateParam;
  double          *curve;
} PositionAnimationData;

static void calcPositionDelta(Position *delta, Position origin, Position target);

static void cleanPositionAnimation(void *data);

static double *generateEasingCurve(unsigned int steps);

static void stepPositionAnimation(unsigned int step, void *data);

void freeAnimation(Animation anim) {
  Animation_ *a = anim;

  if (!a) {
    return;
  }

  // Animation-specific cleanup.
  a->cleanFn(a->data);

  free(a);
}

bool stepAnimation(Animation anim) {
  Animation_ *a = anim;

  if (!a) {
    return false;
  }

  if (!a->loop && a->curStep >= a->steps) {
    return false;
  }

  // Animation-specific step.
  a->stepFn(a->curStep++, a->data);

  return true;
}

Animation makePositionAnimation(Position origin, Position target, unsigned int steps,
                                PositionUpdateFn updateFn, void *param) {
  Animation_            *a;
  PositionAnimationData *data = NULL;
  bool                   ok   = false;

  a = malloc(sizeof(Animation_));

  if (!a) {
    goto cleanup;
  }

  data = malloc(sizeof(PositionAnimationData));

  if (!data) {
    goto cleanup;
  }

  data->curve = generateEasingCurve(steps);

  if (!data->curve) {
    goto cleanup;
  }

  calcPositionDelta(&data->delta, origin, target);
  data->origin      = origin;
  data->updateFn    = updateFn;
  data->updateParam = param;

  a->stepFn  = stepPositionAnimation;
  a->cleanFn = cleanPositionAnimation;
  a->data    = data;
  a->steps   = steps;
  a->curStep = 0;
  a->loop    = false;

  data = NULL;
  ok   = true;

cleanup:
  cleanPositionAnimation(data);

  if (!ok) {
    free(a);
    a = NULL;
  }

  return a;
}

void resetPositionAnimation(Animation anim, Position origin, Position target) {
  Animation_            *a = anim;
  PositionAnimationData *data;

  if (!a) {
    return;
  }

  data = a->data;
  calcPositionDelta(&data->delta, origin, target);
  data->origin = origin;
  a->curStep   = 0;
}

/**
 * @brief Calculation the latitude and longitude deltas for two positions.
 * @param[out] delta  The latitude and longitude deltas.
 * @param[in]  origin The origin position.
 * @param[in]  target The target position.
 */
static void calcPositionDelta(Position *delta, Position origin, Position target) {
  delta->lat = target.lat - origin.lat;
  delta->lon = target.lon - origin.lon;

  // Handle the anti-meridian. The shortest longitudal distance between two
  // points cannot be larger than 180 degrees. If it is, normalize it and switch
  // the sign.
  //
  // For example, W 170 -> E 170 has a delta of 170 - -170 = 340. Subtracting
  // 360 gives a new delta of -20 degrees which would cause the animation to
  // step from W 170 to W 190 (E 170).
  //
  // The reverse case would be E 170 -> W 170 = -170 - 170 = -340. Adding 360
  // gives a new delta of 20 degrees which would cause the animation to step
  // from E 170 to E 190 (W 170).
  if (delta->lon > 180.0) {
    delta->lon -= 360.0;
  } else if (delta->lon < -180.0) {
    delta->lon += 360.0;
  }
}

/**
 * @brief Cleanup a position animation's data.
 * @param[in] data The position animation data.
 */
static void cleanPositionAnimation(void *data) {
  PositionAnimationData *d = data;

  if (!d) {
    return;
  }

  free(d->curve);
  free(d);
}

/**
 * @brief   Generate an easing curve between [0, 1].
 * @details Generates a cosine curve from 0 to 1 over the specified number of
 *          steps.
 *
 *          1                 ____
 *          |              --
 *          |            /
 *          |          /
 *          |       __
 *          0  ----
 *
 * @param[in] steps Number of steps in the animation.
 * @returns An array of @a steps curve values.
 */
static double *generateEasingCurve(unsigned int steps) {
  double *curve;

  if (steps < 2) {
    return NULL;
  }

  curve = malloc(sizeof(double) * steps);

  if (!curve) {
    return NULL;
  }

  for (int i = 0; i < steps; ++i) {
    curve[i] = -(cos(M_PI * (double)i / steps) - 1.0) / 2.0;
  }

  return curve;
}

/**
 * @brief Step a position animation.
 * @param[in] step The current step.
 * @param[in] data The animation data.
 */
static void stepPositionAnimation(unsigned int step, void *data) {
  PositionAnimationData *d = data;
  Position               pos;

  if (!d) {
    return;
  }

  pos.lat = d->origin.lat + (d->delta.lat * d->curve[step]);
  pos.lon = d->origin.lon + (d->delta.lon * d->curve[step]);
  d->updateFn(pos, d->updateParam);
}
