#pragma once
#include <error_code.h>
#include <stddef.h>
#include <stdint.h>

typedef int (*FileIoWriteBytesFunction)(const char *path, const uint8_t *data, size_t size);

int FileIo_WriteBytes(const char *path, const uint8_t *data, size_t size);
void FileIo_SetWriteBytesFunction(FileIoWriteBytesFunction function);
ErrorCode FileIo_EnsureDirectory(const char *path);
