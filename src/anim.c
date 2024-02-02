#include "anim.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

typedef void *AnimationData;

typedef void (*AnimationStepFn)(unsigned int step, AnimationData data);

typedef void (*AnimationCleanFn)(AnimationData data);

typedef struct {
  AnimationStepFn stepFn;
  AnimationCleanFn cleanFn;
  AnimationData data;
  unsigned int steps;
  unsigned int curStep;
} Animation_;

typedef struct {
  Position origin;
  double latDelta;
  double lonDelta;
  PositionUpdateFn updateFn;
  void *updateParam;
  double *curve;
} PositionAnimationData;

static void calcPositionDeltas(Position a, Position b, double *latDelta, double *lonDelta);

static void cleanPositionAnimation(void *data);

static double *generateEasingCurve(unsigned int steps);

static void stepPositionAnimation(unsigned int step, void *data);

void freeAnimation(Animation anim) {
  Animation_ *a = anim;

  if (!a) {
    return;
  }

  a->cleanFn(a->data);

  free(a);
}

Animation makePositionAnimation(Position origin, Position target, unsigned int steps,
                                PositionUpdateFn updateFn, void *param) {
  PositionAnimationData *data = NULL;
  Animation_ *a;

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

  calcPositionDeltas(origin, target, &data->latDelta, &data->lonDelta);
  data->origin = origin;
  data->updateFn = updateFn;
  data->updateParam = param;

  a->stepFn = stepPositionAnimation;
  a->cleanFn = cleanPositionAnimation;
  a->data = data;
  a->steps = steps;
  a->curStep = 0;

  return a;

cleanup:
  cleanPositionAnimation(data);
  free(a);
  return NULL;
}

void resetPositionAnimation(Animation anim, Position origin, Position target) {
  Animation_ *a = anim;
  PositionAnimationData *data;

  if (!a) {
    return;
  }

  data = a->data;
  calcPositionDeltas(origin, target, &data->latDelta, &data->lonDelta);
  a->curStep = 0;
}

bool stepAnimation(Animation anim) {
  Animation_ *a = anim;

  if (!a) {
    return false;
  }

  a->stepFn(a->curStep, a->data);

  return (++(a->curStep) == a->steps);
}

static void calcPositionDeltas(Position a, Position b, double *latDelta, double *lonDelta) {
  *latDelta = b.lat - a.lat;
  *lonDelta = b.lon - a.lon;

  if (*lonDelta > 180.0) {
    *lonDelta -= 360.0;
  } else if (*lonDelta < -180.0) {
    *lonDelta += 360.0;
  }
}

static void cleanPositionAnimation(void *data) {
  PositionAnimationData *d = data;

  if (!d) {
    return;
  }

  free(d->curve);
  free(d);
}

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

static void stepPositionAnimation(unsigned int step, void *data) {
  PositionAnimationData *d = data;
  Position pos;

  if (!d) {
    return;
  }

  pos.lat = d->origin.lat + (d->latDelta * d->curve[step]);
  pos.lon = d->origin.lon + (d->lonDelta * d->curve[step]);
  d->updateFn(pos, d->updateParam);
}
