#if !defined ANIM_H
#define ANIM_H

#include "geo.h"

typedef void *Animation;

typedef void (*PositionUpdateFn)(Position pos, void *param);

void freeAnimation(Animation anim);

Animation makePositionAnimation(Position origin, Position target, unsigned int steps,
                                PositionUpdateFn updateFn, void *param);

void resetPositionAnimation(Animation anim, Position origin, Position target);

bool stepAnimation(Animation anim);

#endif
