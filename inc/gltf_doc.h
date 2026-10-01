#pragma once
#include <error_code.h>
#include <stddef.h>
#include <transform.h>

typedef struct GltfDoc GltfDoc;

typedef enum AnimationPath {
  ANIMATION_PATH_TRANSLATION,
  ANIMATION_PATH_ROTATION,
  ANIMATION_PATH_SCALE,
  ANIMATION_PATH_OTHER
} AnimationPath;

typedef enum Interpolation {
  INTERPOLATION_LINEAR,
  INTERPOLATION_STEP,
  INTERPOLATION_CUBICSPLINE
} Interpolation;

typedef struct GltfChannel {
  size_t node;
  AnimationPath path;
  size_t sampler;
} GltfChannel;

#define GLTF_NO_NODE ((size_t)-1)

GltfDoc *GltfDoc_Load(const char *path, ErrorCode *error);
void GltfDoc_Destroy(GltfDoc *self);
const char *GltfDoc_Version(GltfDoc *self);
const char *GltfDoc_Generator(GltfDoc *self);

size_t GltfDoc_NodeCount(GltfDoc *self);
const char *GltfDoc_NodeName(GltfDoc *self, size_t node);
size_t GltfDoc_NodeParent(GltfDoc *self, size_t node);
size_t GltfDoc_NodeChildCount(GltfDoc *self, size_t node);
size_t GltfDoc_NodeChild(GltfDoc *self, size_t node, size_t child);
int GltfDoc_NodeHasMesh(GltfDoc *self, size_t node);
int GltfDoc_NodeHasSkin(GltfDoc *self, size_t node);
ErrorCode GltfDoc_NodeRest(GltfDoc *self, size_t node, Transform *dest);
void GltfDoc_NodeRestWorldMatrix(GltfDoc *self, size_t node, mat4 dest);

size_t GltfDoc_SkinCount(GltfDoc *self);
const char *GltfDoc_SkinName(GltfDoc *self, size_t skin);
size_t GltfDoc_SkinJointCount(GltfDoc *self, size_t skin);
size_t GltfDoc_SkinJoint(GltfDoc *self, size_t skin, size_t joint);
int GltfDoc_SkinHasInverseBindMatrices(GltfDoc *self, size_t skin);
size_t GltfDoc_SkinInverseBindAccessor(GltfDoc *self, size_t skin);
void GltfDoc_SkinInverseBindMatrix(GltfDoc *self, size_t skin, size_t joint, float dest[16]);
size_t GltfDoc_ArmatureRoot(GltfDoc *self, size_t skin);

size_t GltfDoc_AnimationCount(GltfDoc *self);
const char *GltfDoc_AnimationName(GltfDoc *self, size_t animation);
size_t GltfDoc_ChannelCount(GltfDoc *self, size_t animation);
GltfChannel GltfDoc_Channel(GltfDoc *self, size_t animation, size_t channel);
size_t GltfDoc_SamplerCount(GltfDoc *self, size_t animation);
Interpolation GltfDoc_SamplerInterpolation(GltfDoc *self, size_t animation, size_t sampler);
size_t GltfDoc_SamplerKeyCount(GltfDoc *self, size_t animation, size_t sampler);
size_t GltfDoc_SamplerOutputFloatCount(GltfDoc *self, size_t animation, size_t sampler);
void GltfDoc_SamplerInput(GltfDoc *self, size_t animation, size_t sampler, float *dest);
void GltfDoc_SamplerOutput(GltfDoc *self, size_t animation, size_t sampler, float *dest);
