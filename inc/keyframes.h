#pragma once
#include <gltf_doc.h>
#include <stddef.h>

typedef struct Keyframes {
  Interpolation interpolation;
  size_t key_count;
  size_t components;
  const float *times;
  const float *values;
} Keyframes;

void Keyframes_Evaluate(const Keyframes *self, float time, float *dest);
