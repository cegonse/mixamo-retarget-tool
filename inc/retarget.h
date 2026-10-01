#pragma once
#include <bone_map.h>
#include <frame_align.h>
#include <pose.h>
#include <skeleton.h>

typedef struct RetargetOptions {
  int frame_align;
  int has_frame_rotation;
  vec3 frame_rotation_degrees;
  int has_frame_scale;
  float frame_scale;
  int rest_align;
  int in_place;
  vec3 source_up;
} RetargetOptions;

typedef struct Retarget Retarget;

void RetargetOptions_Default(RetargetOptions *dest);
Retarget *Retarget_Create(const Skeleton *source, const Skeleton *destination, const BoneMap *map,
  const RetargetOptions *options, ErrorCode *error);
void Retarget_Destroy(Retarget *self);
const FrameAlignment *Retarget_Alignment(const Retarget *self);
void Retarget_BeginTrack(Retarget *self);
ErrorCode Retarget_Frame(Retarget *self, Pose *source_pose, Pose *destination_pose);
