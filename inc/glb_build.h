#pragma once
#include <animation_clip.h>
#include <byte_buffer.h>
#include <gltf_doc.h>
#include <json.h>
#include <skeleton.h>

typedef enum GltfComponentType {
  GLTF_COMPONENT_FLOAT = 5126
} GltfComponentType;

typedef struct GlbBuild {
  GltfDoc *doc;
  const Skeleton *skeleton;
  const AnimationClip *clip;
  size_t *output_index;
  size_t output_node_count;
  json_object *root;
  json_object *accessors;
  json_object *buffer_views;
  ByteBuffer *bin;
  ErrorCode error;
} GlbBuild;

void GlbBuild_AddFloat(GlbBuild *self, json_object *array, float value);
json_object *GlbBuild_FloatArray(GlbBuild *self, const float *values, size_t count);
size_t GlbBuild_AddAccessor(GlbBuild *self, const float *values, size_t count, const char *type,
  size_t components, int with_bounds);
void GlbBuild_AddNodes(GlbBuild *self);
void GlbBuild_AddSkin(GlbBuild *self);
void GlbBuild_AddAnimation(GlbBuild *self);
