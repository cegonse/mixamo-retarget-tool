#include <glb_container.h>

enum { JSON_PADDING = 0x20, BIN_PADDING = 0x00 };

static size_t padded(size_t size) {
  return (size + 3) & ~(size_t)3;
}

static ErrorCode appendChunk(ByteBuffer *output, GlbChunkType type, const uint8_t *data,
    size_t size, uint8_t padding) {
  ErrorCode error = ByteBuffer_AppendUint32(output, (uint32_t)padded(size));
  if (error == ERR_NONE) {
    error = ByteBuffer_AppendUint32(output, (uint32_t)type);
  }
  if (error == ERR_NONE) {
    error = ByteBuffer_AppendBytes(output, data, size);
  }
  if (error == ERR_NONE) {
    error = ByteBuffer_PadTo4(output, padding);
  }
  return error;
}

ByteBuffer *GlbContainer_Frame(const char *json, size_t json_length, const ByteBuffer *bin,
    ErrorCode *error) {
  ByteBuffer *output = ByteBuffer_Create();
  size_t total = GLB_HEADER_SIZE + GLB_CHUNK_HEADER_SIZE + padded(json_length)
    + GLB_CHUNK_HEADER_SIZE + padded(ByteBuffer_Size(bin));
  if (output == NULL) {
    *error = ERR_INTERNAL;
    return NULL;
  }
  *error = ByteBuffer_AppendUint32(output, GLB_MAGIC);
  if (*error == ERR_NONE) {
    *error = ByteBuffer_AppendUint32(output, GLB_VERSION);
  }
  if (*error == ERR_NONE) {
    *error = ByteBuffer_AppendUint32(output, (uint32_t)total);
  }
  if (*error == ERR_NONE) {
    *error = appendChunk(output, GLB_CHUNK_JSON, (const uint8_t *)json, json_length, JSON_PADDING);
  }
  if (*error == ERR_NONE) {
    *error = appendChunk(output, GLB_CHUNK_BIN, ByteBuffer_Data(bin), ByteBuffer_Size(bin),
      BIN_PADDING);
  }
  if (*error != ERR_NONE) {
    ByteBuffer_Destroy(output);
    return NULL;
  }
  return output;
}
