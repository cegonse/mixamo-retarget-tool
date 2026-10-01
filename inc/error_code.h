#pragma once

typedef enum ErrorCode {
  ERR_NONE = 0,
  ERR_BAD_ARGS = 1,
  ERR_OPEN_INPUT = 2,
  ERR_BAD_GLB = 3,
  ERR_NOT_FOUND = 4,
  ERR_WRITE_OUTPUT = 5,
  ERR_INTERNAL = 6
} ErrorCode;
