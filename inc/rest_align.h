#pragma once
#include <bone_map.h>
#include <cglm/cglm.h>
#include <skeleton.h>

void RestAlign_Identity(size_t pair_count, versor *corrections);
void RestAlign_Compute(const BoneMap *map, const Skeleton *source, const Skeleton *destination,
  versor frame_rotation, versor *corrections);
