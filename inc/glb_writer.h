#pragma once
#include <animation_clip.h>
#include <byte_buffer.h>
#include <gltf_doc.h>
#include <skeleton.h>

#define GLB_WRITER_GENERATOR "anim-retarget 0.1"

ByteBuffer *GlbWriter_Build(GltfDoc *destination_doc, const Skeleton *destination,
  const AnimationClip *clip, ErrorCode *error);
ErrorCode GlbWriter_Write(const char *path, GltfDoc *destination_doc, const Skeleton *destination,
  const AnimationClip *clip, size_t *bytes_written);
