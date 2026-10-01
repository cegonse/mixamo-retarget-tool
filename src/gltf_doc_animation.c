#include <cgltf.h>
#include <gltf_doc.h>
#include <gltf_doc_data.h>

static cgltf_animation *animationAt(GltfDoc *self, size_t animation) {
  return &GltfDoc_Data(self)->animations[animation];
}

static cgltf_animation_sampler *samplerAt(GltfDoc *self, size_t animation, size_t sampler) {
  return &animationAt(self, animation)->samplers[sampler];
}

size_t GltfDoc_AnimationCount(GltfDoc *self) {
  return GltfDoc_Data(self)->animations_count;
}

const char *GltfDoc_AnimationName(GltfDoc *self, size_t animation) {
  const char *name = animationAt(self, animation)->name;
  return name != NULL ? name : "";
}

size_t GltfDoc_ChannelCount(GltfDoc *self, size_t animation) {
  return animationAt(self, animation)->channels_count;
}

static AnimationPath pathFromCgltf(cgltf_animation_path_type path) {
  switch (path) {
  case cgltf_animation_path_type_translation:
    return ANIMATION_PATH_TRANSLATION;
  case cgltf_animation_path_type_rotation:
    return ANIMATION_PATH_ROTATION;
  case cgltf_animation_path_type_scale:
    return ANIMATION_PATH_SCALE;
  default:
    return ANIMATION_PATH_OTHER;
  }
}

GltfChannel GltfDoc_Channel(GltfDoc *self, size_t animation, size_t channel) {
  cgltf_animation *source = animationAt(self, animation);
  cgltf_animation_channel *entry = &source->channels[channel];
  GltfChannel result;
  result.node = entry->target_node != NULL
    ? cgltf_node_index(GltfDoc_Data(self), entry->target_node) : GLTF_NO_NODE;
  result.path = pathFromCgltf(entry->target_path);
  result.sampler = (size_t)(entry->sampler - source->samplers);
  return result;
}

size_t GltfDoc_SamplerCount(GltfDoc *self, size_t animation) {
  return animationAt(self, animation)->samplers_count;
}

Interpolation GltfDoc_SamplerInterpolation(GltfDoc *self, size_t animation, size_t sampler) {
  switch (samplerAt(self, animation, sampler)->interpolation) {
  case cgltf_interpolation_type_step:
    return INTERPOLATION_STEP;
  case cgltf_interpolation_type_cubic_spline:
    return INTERPOLATION_CUBICSPLINE;
  default:
    return INTERPOLATION_LINEAR;
  }
}

size_t GltfDoc_SamplerKeyCount(GltfDoc *self, size_t animation, size_t sampler) {
  return samplerAt(self, animation, sampler)->input->count;
}

size_t GltfDoc_SamplerOutputFloatCount(GltfDoc *self, size_t animation, size_t sampler) {
  cgltf_accessor *output = samplerAt(self, animation, sampler)->output;
  return output->count * cgltf_num_components(output->type);
}

void GltfDoc_SamplerInput(GltfDoc *self, size_t animation, size_t sampler, float *dest) {
  cgltf_accessor *input = samplerAt(self, animation, sampler)->input;
  cgltf_accessor_unpack_floats(input, dest, input->count);
}

void GltfDoc_SamplerOutput(GltfDoc *self, size_t animation, size_t sampler, float *dest) {
  cgltf_accessor *output = samplerAt(self, animation, sampler)->output;
  cgltf_accessor_unpack_floats(output, dest,
    GltfDoc_SamplerOutputFloatCount(self, animation, sampler));
}
