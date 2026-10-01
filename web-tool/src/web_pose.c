#include <stdlib.h>
#include <string.h>
#include <web_pose.h>

struct WebPoseSet {
  size_t frame_count;
  size_t joint_count;
  float fps;
  float *values;
};

static void loadLocals(const AnimationClip *clip, size_t frame, Pose *pose) {
  size_t joint;
  for (joint = 0; joint < AnimationClip_JointCount(clip); joint++) {
    Transform *local = Pose_Local(pose, joint);
    memcpy(local->translation, AnimationClip_Translations(clip, joint) + frame * 3,
      sizeof local->translation);
    memcpy(local->rotation, AnimationClip_Rotations(clip, joint) + frame * 4,
      sizeof local->rotation);
    memcpy(local->scale, AnimationClip_Scale(clip, joint), sizeof local->scale);
  }
}

static void storeGlobals(WebPoseSet *self, size_t frame, Pose *pose, const Skeleton *skeleton) {
  size_t joint;
  for (joint = 0; joint < self->joint_count; joint++) {
    const Transform *global = Pose_Global(pose, joint);
    size_t slot_index = frame * self->joint_count + Skeleton_JointSkinIndex(skeleton, joint);
    float *slot = &self->values[slot_index * WEB_POSE_FLOATS_PER_JOINT];
    memcpy(slot, global->translation, sizeof global->translation);
    memcpy(slot + 3, global->rotation, sizeof global->rotation);
    memcpy(slot + 7, global->scale, sizeof global->scale);
  }
}

WebPoseSet *WebPoseSet_FromClip(const AnimationClip *clip, const Skeleton *skeleton) {
  WebPoseSet *self = calloc(1, sizeof *self);
  Pose *pose = Pose_Create(Skeleton_JointCount(skeleton));
  size_t frame;
  if (self == NULL || pose == NULL
      || AnimationClip_JointCount(clip) != Skeleton_JointCount(skeleton)) {
    Pose_Destroy(pose);
    WebPoseSet_Destroy(self);
    return NULL;
  }
  self->frame_count = AnimationClip_FrameCount(clip);
  self->joint_count = Skeleton_JointCount(skeleton);
  self->fps = AnimationClip_Fps(clip);
  self->values = calloc(self->frame_count * self->joint_count * WEB_POSE_FLOATS_PER_JOINT + 1,
    sizeof *self->values);
  if (self->values == NULL) {
    Pose_Destroy(pose);
    WebPoseSet_Destroy(self);
    return NULL;
  }
  for (frame = 0; frame < self->frame_count; frame++) {
    loadLocals(clip, frame, pose);
    Pose_ComputeGlobals(pose, skeleton);
    storeGlobals(self, frame, pose, skeleton);
  }
  Pose_Destroy(pose);
  return self;
}

void WebPoseSet_Destroy(WebPoseSet *self) {
  if (self == NULL) {
    return;
  }
  free(self->values);
  free(self);
}

size_t WebPoseSet_FrameCount(const WebPoseSet *self) {
  return self->frame_count;
}

size_t WebPoseSet_JointCount(const WebPoseSet *self) {
  return self->joint_count;
}

float WebPoseSet_Fps(const WebPoseSet *self) {
  return self->fps;
}

const float *WebPoseSet_Frame(const WebPoseSet *self, size_t frame) {
  return &self->values[frame * self->joint_count * WEB_POSE_FLOATS_PER_JOINT];
}

const float *WebPoseSet_Joint(const WebPoseSet *self, size_t frame, size_t skin_index) {
  return WebPoseSet_Frame(self, frame) + skin_index * WEB_POSE_FLOATS_PER_JOINT;
}
