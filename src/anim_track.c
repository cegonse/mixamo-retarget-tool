#include <anim_track.h>
#include <keyframes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <track_timing.h>

enum { PATH_COUNT = 3 };

typedef struct JointChannels {
  Keyframes paths[PATH_COUNT];
  Transform rest;
} JointChannels;

struct AnimTrack {
  char *name;
  TrackTiming timing;
  size_t joint_count;
  JointChannels *joints;
};

static const size_t path_components[PATH_COUNT] = {3, 4, 3};

static size_t expectedFloatCount(Interpolation interpolation, size_t keys, size_t components) {
  return keys * components * (interpolation == INTERPOLATION_CUBICSPLINE ? 3 : 1);
}

static ErrorCode loadKeyframes(GltfDoc *doc, size_t animation, GltfChannel channel,
    Keyframes *dest) {
  size_t keys = GltfDoc_SamplerKeyCount(doc, animation, channel.sampler);
  size_t components = path_components[channel.path];
  Interpolation interpolation = GltfDoc_SamplerInterpolation(doc, animation, channel.sampler);
  size_t float_count = GltfDoc_SamplerOutputFloatCount(doc, animation, channel.sampler);
  float *times, *values;
  if (keys == 0 || float_count != expectedFloatCount(interpolation, keys, components)) {
    fprintf(stderr, "error: animation %zu: sampler %zu has %zu output floats for %zu keys\n",
      animation, channel.sampler, float_count, keys);
    return ERR_BAD_GLB;
  }
  times = malloc(keys * sizeof *times);
  values = malloc(float_count * sizeof *values);
  dest->times = times;
  dest->values = values;
  if (times == NULL || values == NULL) {
    return ERR_INTERNAL;
  }
  GltfDoc_SamplerInput(doc, animation, channel.sampler, times);
  GltfDoc_SamplerOutput(doc, animation, channel.sampler, values);
  dest->interpolation = interpolation;
  dest->key_count = keys;
  dest->components = components;
  return ERR_NONE;
}

static ErrorCode loadChannel(AnimTrack *self, GltfDoc *doc, size_t animation,
    const Skeleton *skeleton, size_t channel) {
  GltfChannel entry = GltfDoc_Channel(doc, animation, channel);
  size_t joint = Skeleton_FindJointByNode(skeleton, entry.node);
  Keyframes *slot;
  if (entry.path == ANIMATION_PATH_OTHER || joint == SKELETON_NO_JOINT) {
    return ERR_NONE;
  }
  slot = &self->joints[joint].paths[entry.path];
  if (slot->key_count > 0) {
    return ERR_NONE;
  }
  return loadKeyframes(doc, animation, entry, slot);
}

static ErrorCode loadTrack(AnimTrack *self, GltfDoc *doc, size_t animation,
    const Skeleton *skeleton) {
  size_t joint, channel;
  ErrorCode error = TrackTiming_FromDoc(doc, animation, &self->timing);
  const char *name = GltfDoc_AnimationName(doc, animation);
  self->name = malloc(strlen(name) + 1);
  self->joint_count = Skeleton_JointCount(skeleton);
  self->joints = calloc(self->joint_count > 0 ? self->joint_count : 1, sizeof *self->joints);
  if (self->name == NULL || self->joints == NULL) {
    return ERR_INTERNAL;
  }
  strcpy(self->name, name);
  for (joint = 0; joint < self->joint_count; joint++) {
    self->joints[joint].rest = *Skeleton_RestLocal(skeleton, joint);
  }
  for (channel = 0; channel < GltfDoc_ChannelCount(doc, animation) && error == ERR_NONE; channel++) {
    error = loadChannel(self, doc, animation, skeleton, channel);
  }
  return error;
}

AnimTrack *AnimTrack_FromDoc(GltfDoc *doc, size_t animation, const Skeleton *skeleton,
    ErrorCode *error) {
  AnimTrack *self;
  if (animation >= GltfDoc_AnimationCount(doc)) {
    *error = ERR_NOT_FOUND;
    return NULL;
  }
  self = calloc(1, sizeof *self);
  if (self == NULL) {
    *error = ERR_INTERNAL;
    return NULL;
  }
  *error = loadTrack(self, doc, animation, skeleton);
  if (*error != ERR_NONE) {
    AnimTrack_Destroy(self);
    return NULL;
  }
  return self;
}

void AnimTrack_Destroy(AnimTrack *self) {
  size_t joint, path;
  if (self == NULL) {
    return;
  }
  for (joint = 0; self->joints != NULL && joint < self->joint_count; joint++) {
    for (path = 0; path < PATH_COUNT; path++) {
      free((float *)self->joints[joint].paths[path].times);
      free((float *)self->joints[joint].paths[path].values);
    }
  }
  free(self->joints);
  free(self->name);
  free(self);
}

const char *AnimTrack_Name(const AnimTrack *self) {
  return self->name;
}

float AnimTrack_Fps(const AnimTrack *self) {
  return self->timing.fps;
}

float AnimTrack_Start(const AnimTrack *self) {
  return self->timing.start;
}

float AnimTrack_End(const AnimTrack *self) {
  return self->timing.end;
}

size_t AnimTrack_FrameCount(const AnimTrack *self, float fps) {
  return (size_t)lroundf((self->timing.end - self->timing.start) * fps) + 1;
}

static void evaluatePath(const Keyframes *keyframes, float time, float *rest, float *dest,
    size_t components) {
  if (keyframes->key_count == 0) {
    memcpy(dest, rest, components * sizeof *dest);
    return;
  }
  Keyframes_Evaluate(keyframes, time, dest);
}

void AnimTrack_EvaluateLocal(const AnimTrack *self, size_t joint, float time, Transform *dest) {
  JointChannels channels = self->joints[joint];
  evaluatePath(&channels.paths[ANIMATION_PATH_TRANSLATION], time, channels.rest.translation,
    dest->translation, 3);
  evaluatePath(&channels.paths[ANIMATION_PATH_ROTATION], time, channels.rest.rotation,
    dest->rotation, 4);
  evaluatePath(&channels.paths[ANIMATION_PATH_SCALE], time, channels.rest.scale, dest->scale, 3);
  glm_quat_normalize(dest->rotation);
}

void AnimTrack_EvaluatePose(const AnimTrack *self, float time, Pose *pose) {
  size_t joint;
  for (joint = 0; joint < self->joint_count && joint < Pose_JointCount(pose); joint++) {
    AnimTrack_EvaluateLocal(self, joint, time, Pose_Local(pose, joint));
  }
}
