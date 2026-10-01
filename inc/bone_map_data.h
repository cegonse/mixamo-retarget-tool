#pragma once
#include <bone_map.h>

typedef struct BonePair {
  char *source_name;
  char *destination_name;
  size_t source_joint;
  size_t destination_joint;
  size_t mapped_parent;
} BonePair;

struct BoneMap {
  BonePair *pairs;
  size_t pair_count;
  size_t capacity;
  char error_message[512];
};

ErrorCode BoneMap_Fail(BoneMap *self, ErrorCode error, const char *format, const char *name);
