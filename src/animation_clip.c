#include <animation_clip.h>
#include <stdlib.h>
#include <string.h>

struct AnimationClip {
  char *name;
  size_t joint_count;
  size_t frame_count;
  float fps;
  float *translations;
  float *rotations;
  float *scales;
};

AnimationClip *AnimationClip_Create(const char *name, size_t joint_count, size_t frame_count,
    float fps) {
  AnimationClip *self = calloc(1, sizeof *self);
  size_t samples = (joint_count > 0 ? joint_count : 1) * (frame_count > 0 ? frame_count : 1);
  if (self == NULL) {
    return NULL;
  }
  self->name = malloc(strlen(name) + 1);
  self->translations = calloc(samples * 3, sizeof *self->translations);
  self->rotations = calloc(samples * 4, sizeof *self->rotations);
  self->scales = calloc((joint_count > 0 ? joint_count : 1) * 3, sizeof *self->scales);
  if (self->name == NULL || self->translations == NULL || self->rotations == NULL
      || self->scales == NULL) {
    AnimationClip_Destroy(self);
    return NULL;
  }
  strcpy(self->name, name);
  self->joint_count = joint_count;
  self->frame_count = frame_count;
  self->fps = fps;
  return self;
}

void AnimationClip_Destroy(AnimationClip *self) {
  if (self == NULL) {
    return;
  }
  free(self->name);
  free(self->translations);
  free(self->rotations);
  free(self->scales);
  free(self);
}

void AnimationClip_SetFrame(AnimationClip *self, size_t frame, Pose *pose) {
  size_t joint;
  for (joint = 0; joint < self->joint_count; joint++) {
    Transform *local = Pose_Local(pose, joint);
    size_t sample = joint * self->frame_count + frame;
    memcpy(&self->translations[sample * 3], local->translation, 3 * sizeof(float));
    memcpy(&self->rotations[sample * 4], local->rotation, 4 * sizeof(float));
    if (frame == 0) {
      memcpy(&self->scales[joint * 3], local->scale, 3 * sizeof(float));
    }
  }
}

const char *AnimationClip_Name(const AnimationClip *self) {
  return self->name;
}

size_t AnimationClip_JointCount(const AnimationClip *self) {
  return self->joint_count;
}

size_t AnimationClip_FrameCount(const AnimationClip *self) {
  return self->frame_count;
}

float AnimationClip_Fps(const AnimationClip *self) {
  return self->fps;
}

float AnimationClip_FrameTime(const AnimationClip *self, size_t frame) {
  return (float)frame / self->fps;
}

float AnimationClip_Duration(const AnimationClip *self) {
  return self->frame_count > 0 ? AnimationClip_FrameTime(self, self->frame_count - 1) : 0.0f;
}

const float *AnimationClip_Translations(const AnimationClip *self, size_t joint) {
  return &self->translations[joint * self->frame_count * 3];
}

const float *AnimationClip_Rotations(const AnimationClip *self, size_t joint) {
  return &self->rotations[joint * self->frame_count * 4];
}

const float *AnimationClip_Scale(const AnimationClip *self, size_t joint) {
  return &self->scales[joint * 3];
}
