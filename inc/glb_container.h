#pragma once
#include <byte_buffer.h>

typedef enum GlbChunkType {
  GLB_CHUNK_JSON = 0x4E4F534A,
  GLB_CHUNK_BIN = 0x004E4942
} GlbChunkType;

typedef enum GlbHeader {
  GLB_MAGIC = 0x46546C67,
  GLB_VERSION = 2,
  GLB_HEADER_SIZE = 12,
  GLB_CHUNK_HEADER_SIZE = 8
} GlbHeader;

ByteBuffer *GlbContainer_Frame(const char *json, size_t json_length, const ByteBuffer *bin,
  ErrorCode *error);
