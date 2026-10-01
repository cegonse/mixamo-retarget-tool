#include <glb_build.h>
#include <stdlib.h>

typedef struct SharedInputs {
  size_t frame_times;
  size_t scale_times;
  size_t scale_key_count;
} SharedInputs;

static size_t addTimes(GlbBuild *self, const float *times, size_t count) {
  return GlbBuild_AddAccessor(self, times, count, "SCALAR", 1, 1);
}

static SharedInputs addSharedInputs(GlbBuild *self) {
  size_t frame, frame_count = AnimationClip_FrameCount(self->clip);
  float *times = malloc((frame_count > 0 ? frame_count : 1) * sizeof *times);
  float scale_times[2] = {0.0f, AnimationClip_Duration(self->clip)};
  SharedInputs inputs = {0, 0, frame_count > 1 ? 2 : 1};
  if (times == NULL) {
    self->error = ERR_INTERNAL;
    return inputs;
  }
  for (frame = 0; frame < frame_count; frame++) {
    times[frame] = AnimationClip_FrameTime(self->clip, frame);
  }
  inputs.frame_times = addTimes(self, times, frame_count);
  inputs.scale_times = addTimes(self, scale_times, inputs.scale_key_count);
  free(times);
  return inputs;
}

static void addChannel(json_object *channels, json_object *samplers,
    size_t output_node, const char *path) {
  json_object *channel = json_object_new_object();
  json_object *target = json_object_new_object();
  json_object_object_add(channel, "sampler",
    json_object_new_int64((int64_t)json_object_array_length(samplers) - 1));
  json_object_object_add(target, "node", json_object_new_int64((int64_t)output_node));
  json_object_object_add(target, "path", json_object_new_string(path));
  json_object_object_add(channel, "target", target);
  json_object_array_add(channels, channel);
}

static void addSampler(json_object *samplers, size_t input, const char *interpolation,
    size_t output) {
  json_object *sampler = json_object_new_object();
  json_object_object_add(sampler, "input", json_object_new_int64((int64_t)input));
  json_object_object_add(sampler, "interpolation", json_object_new_string(interpolation));
  json_object_object_add(sampler, "output", json_object_new_int64((int64_t)output));
  json_object_array_add(samplers, sampler);
}

static void addJointChannels(GlbBuild *self, const SharedInputs *inputs, size_t skin_joint,
    json_object *channels, json_object *samplers) {
  size_t node = GltfDoc_SkinJoint(self->doc, 0, skin_joint);
  size_t joint = Skeleton_FindJointByNode(self->skeleton, node);
  size_t output_node = self->output_index[node], frames = AnimationClip_FrameCount(self->clip);
  const float *scale = AnimationClip_Scale(self->clip, joint);
  float scale_keys[6] = {scale[0], scale[1], scale[2], scale[0], scale[1], scale[2]};
  addSampler(samplers, inputs->frame_times, "LINEAR", GlbBuild_AddAccessor(self,
    AnimationClip_Translations(self->clip, joint), frames, "VEC3", 3, 0));
  addChannel(channels, samplers, output_node, "translation");
  addSampler(samplers, inputs->frame_times, "LINEAR", GlbBuild_AddAccessor(self,
    AnimationClip_Rotations(self->clip, joint), frames, "VEC4", 4, 0));
  addChannel(channels, samplers, output_node, "rotation");
  addSampler(samplers, inputs->scale_times, "STEP",
    GlbBuild_AddAccessor(self, scale_keys, inputs->scale_key_count, "VEC3", 3, 0));
  addChannel(channels, samplers, output_node, "scale");
}

void GlbBuild_AddAnimation(GlbBuild *self) {
  json_object *animations = json_object_new_array();
  json_object *animation = json_object_new_object();
  json_object *channels = json_object_new_array();
  json_object *samplers = json_object_new_array();
  SharedInputs inputs = addSharedInputs(self);
  size_t skin_joint;
  for (skin_joint = 0; skin_joint < GltfDoc_SkinJointCount(self->doc, 0); skin_joint++) {
    addJointChannels(self, &inputs, skin_joint, channels, samplers);
  }
  json_object_object_add(animation, "channels", channels);
  json_object_object_add(animation, "name", json_object_new_string(AnimationClip_Name(self->clip)));
  json_object_object_add(animation, "samplers", samplers);
  json_object_array_add(animations, animation);
  json_object_object_add(self->root, "animations", animations);
}
