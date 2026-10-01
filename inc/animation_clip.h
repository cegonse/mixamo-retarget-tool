#pragma once
#include <error_code.h>
#include <pose.h>
#include <stddef.h>

typedef struct AnimationClip AnimationClip;

AnimationClip *AnimationClip_Create(const char *name, size_t joint_count, size_t frame_count,
  float fps);
void AnimationClip_Destroy(AnimationClip *self);
void AnimationClip_SetFrame(AnimationClip *self, size_t frame, Pose *pose);
const char *AnimationClip_Name(const AnimationClip *self);
size_t AnimationClip_JointCount(const AnimationClip *self);
size_t AnimationClip_FrameCount(const AnimationClip *self);
float AnimationClip_Fps(const AnimationClip *self);
float AnimationClip_FrameTime(const AnimationClip *self, size_t frame);
float AnimationClip_Duration(const AnimationClip *self);
const float *AnimationClip_Translations(const AnimationClip *self, size_t joint);
const float *AnimationClip_Rotations(const AnimationClip *self, size_t joint);
const float *AnimationClip_Scale(const AnimationClip *self, size_t joint);
