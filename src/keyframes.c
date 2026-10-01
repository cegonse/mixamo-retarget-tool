#include <keyframes.h>
#include <string.h>
#include <transform.h>

enum { ROTATION_COMPONENTS = 4 };

static const float *keyValue(const Keyframes *self, size_t key) {
  size_t stride = self->interpolation == INTERPOLATION_CUBICSPLINE ? 3 : 1;
  size_t offset = self->interpolation == INTERPOLATION_CUBICSPLINE ? 1 : 0;
  return &self->values[(stride * key + offset) * self->components];
}

static const float *cubicTangent(const Keyframes *self, size_t key, size_t slot) {
  return &self->values[(3 * key + slot) * self->components];
}

static size_t segmentStart(const Keyframes *self, float time) {
  size_t key = 0;
  while (key + 2 < self->key_count && self->times[key + 1] <= time) {
    key++;
  }
  return key;
}

static void evaluateLinear(const Keyframes *self, size_t key, float amount, float *dest) {
  size_t component;
  const float *from = keyValue(self, key);
  const float *to = keyValue(self, key + 1);
  if (self->components == ROTATION_COMPONENTS) {
    Transform_QuatSlerp((float *)from, (float *)to, amount, dest);
    return;
  }
  for (component = 0; component < self->components; component++) {
    dest[component] = from[component] + (to[component] - from[component]) * amount;
  }
}

static void evaluateCubic(const Keyframes *self, size_t key, float amount, float *dest) {
  float step = self->times[key + 1] - self->times[key];
  float t2 = amount * amount, t3 = t2 * amount;
  float value_weight = 2 * t3 - 3 * t2 + 1, out_weight = (t3 - 2 * t2 + amount) * step;
  float next_weight = -2 * t3 + 3 * t2, in_weight = (t3 - t2) * step;
  const float *value = keyValue(self, key), *out_tangent = cubicTangent(self, key, 2);
  const float *next = keyValue(self, key + 1), *in_tangent = cubicTangent(self, key + 1, 0);
  size_t component;
  for (component = 0; component < self->components; component++) {
    dest[component] = value_weight * value[component] + out_weight * out_tangent[component]
      + next_weight * next[component] + in_weight * in_tangent[component];
  }
  if (self->components == ROTATION_COMPONENTS) {
    glm_quat_normalize(dest);
  }
}

static void copyKey(const Keyframes *self, size_t key, float *dest) {
  memcpy(dest, keyValue(self, key), self->components * sizeof *dest);
}

void Keyframes_Evaluate(const Keyframes *self, float time, float *dest) {
  size_t key;
  float span, amount;
  if (self->key_count == 1 || time <= self->times[0]) {
    copyKey(self, 0, dest);
    return;
  }
  if (time >= self->times[self->key_count - 1]) {
    copyKey(self, self->key_count - 1, dest);
    return;
  }
  key = segmentStart(self, time);
  if (self->interpolation == INTERPOLATION_STEP) {
    copyKey(self, key, dest);
    return;
  }
  span = self->times[key + 1] - self->times[key];
  amount = span > 0.0f ? (time - self->times[key]) / span : 0.0f;
  if (self->interpolation == INTERPOLATION_CUBICSPLINE) {
    evaluateCubic(self, key, amount, dest);
  } else {
    evaluateLinear(self, key, amount, dest);
  }
}
