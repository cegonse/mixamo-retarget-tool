#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <file_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int writeWithLibc(const char *path, const uint8_t *data, size_t size) {
  FILE *file = fopen(path, "wb");
  size_t written;
  if (file == NULL) {
    return -1;
  }
  written = fwrite(data, 1, size, file);
  if (fclose(file) != 0 || written != size) {
    return -1;
  }
  return 0;
}

static FileIoWriteBytesFunction write_bytes = writeWithLibc;

int FileIo_WriteBytes(const char *path, const uint8_t *data, size_t size) {
  return write_bytes(path, data, size);
}

void FileIo_SetWriteBytesFunction(FileIoWriteBytesFunction function) {
  write_bytes = function != NULL ? function : writeWithLibc;
}

static int makeOne(const char *path) {
  struct stat status;
  if (mkdir(path, 0777) == 0 || (errno == EEXIST && stat(path, &status) == 0
      && S_ISDIR(status.st_mode))) {
    return 0;
  }
  return -1;
}

ErrorCode FileIo_EnsureDirectory(const char *path) {
  size_t length = strlen(path), index;
  char *partial = malloc(length + 1);
  int failed = 0;
  if (partial == NULL) {
    return ERR_INTERNAL;
  }
  memcpy(partial, path, length + 1);
  for (index = 1; index <= length && !failed; index++) {
    if (partial[index] == '/' || partial[index] == '\0') {
      char saved = partial[index];
      partial[index] = '\0';
      failed = makeOne(partial) != 0;
      partial[index] = saved;
    }
  }
  free(partial);
  return failed ? ERR_WRITE_OUTPUT : ERR_NONE;
}
