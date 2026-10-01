#pragma once
#include <bone_map.h>
#include <cglm/cglm.h>
#include <skeleton.h>

typedef struct FrameAlignment {
  versor rotation;
  float scale;
  vec3 translation;
  float rms_residual;
  float destination_spread;
  int degenerate;
} FrameAlignment;

void FrameAlign_Identity(FrameAlignment *dest);
void FrameAlign_Solve(vec3 *source_points, vec3 *destination_points, size_t count,
  FrameAlignment *dest);
ErrorCode FrameAlign_FromMap(const BoneMap *map, const Skeleton *source,
  const Skeleton *destination, FrameAlignment *dest);
