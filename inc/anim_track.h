#pragma once
#include <gltf_doc.h>
#include <pose.h>
#include <skeleton.h>

typedef struct AnimTrack AnimTrack;

AnimTrack *AnimTrack_FromDoc(GltfDoc *doc, size_t animation, const Skeleton *skeleton,
  ErrorCode *error);
void AnimTrack_Destroy(AnimTrack *self);
const char *AnimTrack_Name(const AnimTrack *self);
float AnimTrack_Fps(const AnimTrack *self);
float AnimTrack_Start(const AnimTrack *self);
float AnimTrack_End(const AnimTrack *self);
size_t AnimTrack_FrameCount(const AnimTrack *self, float fps);
void AnimTrack_EvaluateLocal(const AnimTrack *self, size_t joint, float time, Transform *dest);
void AnimTrack_EvaluatePose(const AnimTrack *self, float time, Pose *pose);
