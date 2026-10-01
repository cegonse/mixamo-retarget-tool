#pragma once
#include <animation_clip.h>
#include <skeleton.h>

enum { WEB_POSE_FLOATS_PER_JOINT = 10 };

typedef struct WebPoseSet WebPoseSet;

WebPoseSet *WebPoseSet_FromClip(const AnimationClip *clip, const Skeleton *skeleton);
void WebPoseSet_Destroy(WebPoseSet *self);
size_t WebPoseSet_FrameCount(const WebPoseSet *self);
size_t WebPoseSet_JointCount(const WebPoseSet *self);
float WebPoseSet_Fps(const WebPoseSet *self);
const float *WebPoseSet_Frame(const WebPoseSet *self, size_t frame);
const float *WebPoseSet_Joint(const WebPoseSet *self, size_t frame, size_t skin_index);
