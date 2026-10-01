#include <math.h>
#include <stdlib.h>
#include <track_timing.h>

static const float fallback_fps = 30.0f;

static int compareFloats(const void *left, const void *right) {
  float a = *(const float *)left;
  float b = *(const float *)right;
  return (a > b) - (a < b);
}

static float medianStep(float *times, size_t count) {
  size_t index;
  for (index = 0; index + 1 < count; index++) {
    times[index] = times[index + 1] - times[index];
  }
  qsort(times, count - 1, sizeof times[0], compareFloats);
  return times[(count - 1) / 2];
}

static ErrorCode inferFps(GltfDoc *doc, size_t animation, size_t sampler, size_t count,
    float *fps) {
  float *times;
  float step;
  *fps = fallback_fps;
  if (count < 2) {
    return ERR_NONE;
  }
  times = malloc(count * sizeof *times);
  if (times == NULL) {
    return ERR_INTERNAL;
  }
  GltfDoc_SamplerInput(doc, animation, sampler, times);
  step = medianStep(times, count);
  free(times);
  if (step > 0.0f) {
    *fps = roundf(1.0f / step);
  }
  return ERR_NONE;
}

static void widenRange(GltfDoc *doc, size_t animation, size_t sampler, TrackTiming *dest) {
  size_t count = GltfDoc_SamplerKeyCount(doc, animation, sampler);
  float *times = malloc(count * sizeof *times);
  if (times == NULL || count == 0) {
    free(times);
    return;
  }
  GltfDoc_SamplerInput(doc, animation, sampler, times);
  dest->start = fminf(dest->start, times[0]);
  dest->end = fmaxf(dest->end, times[count - 1]);
  free(times);
}

ErrorCode TrackTiming_FromDoc(GltfDoc *doc, size_t animation, TrackTiming *dest) {
  size_t sampler, longest = 0;
  dest->start = INFINITY;
  dest->end = -INFINITY;
  dest->max_keys = 0;
  for (sampler = 0; sampler < GltfDoc_SamplerCount(doc, animation); sampler++) {
    size_t count = GltfDoc_SamplerKeyCount(doc, animation, sampler);
    widenRange(doc, animation, sampler, dest);
    if (count > dest->max_keys) {
      dest->max_keys = count;
      longest = sampler;
    }
  }
  if (dest->max_keys == 0) {
    dest->start = dest->end = 0.0f;
    dest->fps = fallback_fps;
    return ERR_NONE;
  }
  return inferFps(doc, animation, longest, dest->max_keys, &dest->fps);
}
