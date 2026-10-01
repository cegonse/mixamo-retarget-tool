#pragma once
#include <retarget.h>

struct Retarget {
  const Skeleton *source;
  const Skeleton *destination;
  const BoneMap *map;
  FrameAlignment alignment;
  versor *corrections;
  versor *previous_rotations;
  vec3 up;
  int in_place;
  int has_previous;
};
