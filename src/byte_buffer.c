#include <byte_buffer.h>
#include <stdlib.h>
#include <string.h>

struct ByteBuffer {
  uint8_t *data;
  size_t size;
  size_t capacity;
};

ByteBuffer *ByteBuffer_Create(void) {
  return calloc(1, sizeof(ByteBuffer));
}

void ByteBuffer_Destroy(ByteBuffer *self) {
  if (self == NULL) {
    return;
  }
  free(self->data);
  free(self);
}

static ErrorCode reserve(ByteBuffer *self, size_t extra) {
  size_t capacity = self->capacity > 0 ? self->capacity : 256;
  uint8_t *data;
  if (self->size + extra <= self->capacity) {
    return ERR_NONE;
  }
  while (capacity < self->size + extra) {
    capacity *= 2;
  }
  data = realloc(self->data, capacity);
  if (data == NULL) {
    return ERR_INTERNAL;
  }
  self->data = data;
  self->capacity = capacity;
  return ERR_NONE;
}

ErrorCode ByteBuffer_AppendBytes(ByteBuffer *self, const uint8_t *data, size_t size) {
  ErrorCode error = reserve(self, size);
  if (error == ERR_NONE && size > 0) {
    memcpy(self->data + self->size, data, size);
    self->size += size;
  }
  return error;
}

ErrorCode ByteBuffer_AppendByte(ByteBuffer *self, uint8_t value) {
  return ByteBuffer_AppendBytes(self, &value, 1);
}

ErrorCode ByteBuffer_AppendUint32(ByteBuffer *self, uint32_t value) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)(value & 0xFFu);
  bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
  bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
  bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
  return ByteBuffer_AppendBytes(self, bytes, sizeof bytes);
}

ErrorCode ByteBuffer_AppendFloat(ByteBuffer *self, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof bits);
  return ByteBuffer_AppendUint32(self, bits);
}

ErrorCode ByteBuffer_PadTo4(ByteBuffer *self, uint8_t padding) {
  ErrorCode error = ERR_NONE;
  while (error == ERR_NONE && self->size % 4 != 0) {
    error = ByteBuffer_AppendByte(self, padding);
  }
  return error;
}

const uint8_t *ByteBuffer_Data(const ByteBuffer *self) {
  return self->data;
}

size_t ByteBuffer_Size(const ByteBuffer *self) {
  return self->size;
}
