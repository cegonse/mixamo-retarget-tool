#pragma once
#include <gltf_doc.h>

typedef struct TrackTiming {
  float start;
  float end;
  size_t max_keys;
  float fps;
} TrackTiming;

ErrorCode TrackTiming_FromDoc(GltfDoc *doc, size_t animation, TrackTiming *dest);
