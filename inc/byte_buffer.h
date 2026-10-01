#pragma once
#include <error_code.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ByteBuffer ByteBuffer;

ByteBuffer *ByteBuffer_Create(void);
void ByteBuffer_Destroy(ByteBuffer *self);
ErrorCode ByteBuffer_AppendByte(ByteBuffer *self, uint8_t value);
ErrorCode ByteBuffer_AppendUint32(ByteBuffer *self, uint32_t value);
ErrorCode ByteBuffer_AppendFloat(ByteBuffer *self, float value);
ErrorCode ByteBuffer_AppendBytes(ByteBuffer *self, const uint8_t *data, size_t size);
ErrorCode ByteBuffer_PadTo4(ByteBuffer *self, uint8_t padding);
const uint8_t *ByteBuffer_Data(const ByteBuffer *self);
size_t ByteBuffer_Size(const ByteBuffer *self);
