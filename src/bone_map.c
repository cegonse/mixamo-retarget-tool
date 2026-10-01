#include <bone_map.h>
#include <bone_map_data.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_LINE_LENGTH = 1024 };

BoneMap *BoneMap_Create(void) {
  return calloc(1, sizeof(BoneMap));
}

void BoneMap_Destroy(BoneMap *self) {
  size_t pair;
  if (self == NULL) {
    return;
  }
  for (pair = 0; pair < self->pair_count; pair++) {
    free(self->pairs[pair].source_name);
    free(self->pairs[pair].destination_name);
  }
  free(self->pairs);
  free(self);
}

const char *BoneMap_ErrorMessage(const BoneMap *self) {
  return self->error_message;
}

ErrorCode BoneMap_Fail(BoneMap *self, ErrorCode error, const char *format, const char *name) {
  snprintf(self->error_message, sizeof self->error_message, format, name);
  return error;
}

static char *copyRange(const char *start, const char *end) {
  char *copy;
  while (start < end && isspace((unsigned char)*start)) {
    start++;
  }
  while (end > start && isspace((unsigned char)end[-1])) {
    end--;
  }
  copy = malloc((size_t)(end - start) + 1);
  if (copy != NULL) {
    memcpy(copy, start, (size_t)(end - start));
    copy[end - start] = '\0';
  }
  return copy;
}

static size_t findSourceName(const BoneMap *self, const char *name) {
  size_t pair;
  for (pair = 0; pair < self->pair_count; pair++) {
    if (strcmp(self->pairs[pair].source_name, name) == 0) {
      return pair;
    }
  }
  return BONE_MAP_NO_PAIR;
}

static ErrorCode growPairs(BoneMap *self) {
  size_t capacity = self->capacity > 0 ? self->capacity * 2 : 32;
  BonePair *pairs = realloc(self->pairs, capacity * sizeof *pairs);
  if (pairs == NULL) {
    return ERR_INTERNAL;
  }
  self->pairs = pairs;
  self->capacity = capacity;
  return ERR_NONE;
}

static ErrorCode storePair(BoneMap *self, char *source_name, char *destination_name) {
  size_t existing = findSourceName(self, source_name);
  BonePair *pair;
  if (existing != BONE_MAP_NO_PAIR) {
    free(source_name);
    free(self->pairs[existing].destination_name);
    self->pairs[existing].destination_name = destination_name;
    return ERR_NONE;
  }
  if (self->pair_count == self->capacity && growPairs(self) != ERR_NONE) {
    free(source_name);
    free(destination_name);
    return ERR_INTERNAL;
  }
  pair = &self->pairs[self->pair_count++];
  pair->source_name = source_name;
  pair->destination_name = destination_name;
  pair->source_joint = pair->destination_joint = SKELETON_NO_JOINT;
  pair->mapped_parent = BONE_MAP_NO_PAIR;
  return ERR_NONE;
}

static ErrorCode addEntry(BoneMap *self, const char *start, const char *end) {
  const char *equals = memchr(start, '=', (size_t)(end - start));
  char *source_name, *destination_name, *entry = copyRange(start, end);
  if (entry == NULL) {
    return ERR_INTERNAL;
  }
  if (equals == NULL) {
    BoneMap_Fail(self, ERR_BAD_ARGS, "bad map entry \"%s\": expected src=dst", entry);
    free(entry);
    return ERR_BAD_ARGS;
  }
  source_name = copyRange(start, equals);
  destination_name = copyRange(equals + 1, end);
  if (source_name == NULL || destination_name == NULL || !*source_name || !*destination_name) {
    BoneMap_Fail(self, ERR_BAD_ARGS, "bad map entry \"%s\": empty bone name", entry);
    free(entry);
    free(source_name);
    free(destination_name);
    return ERR_BAD_ARGS;
  }
  free(entry);
  return storePair(self, source_name, destination_name);
}

static int isBlank(const char *start, const char *end) {
  while (start < end && isspace((unsigned char)*start)) {
    start++;
  }
  return start == end;
}

ErrorCode BoneMap_AddPairs(BoneMap *self, const char *text) {
  const char *start = text;
  ErrorCode error = ERR_NONE;
  while (error == ERR_NONE) {
    const char *end = strchr(start, ',');
    if (end == NULL) {
      end = start + strlen(start);
    }
    if (!isBlank(start, end)) {
      error = addEntry(self, start, end);
    }
    if (*end == '\0') {
      break;
    }
    start = end + 1;
  }
  return error;
}

static ErrorCode addLine(BoneMap *self, const char *line) {
  const char *end = strchr(line, '#');
  if (end == NULL) {
    end = line + strlen(line);
  }
  if (isBlank(line, end)) {
    return ERR_NONE;
  }
  return addEntry(self, line, end);
}

ErrorCode BoneMap_AddText(BoneMap *self, const char *text) {
  char line[MAX_LINE_LENGTH];
  const char *start = text;
  ErrorCode error = ERR_NONE;
  while (error == ERR_NONE && *start != '\0') {
    const char *end = strchr(start, '\n');
    size_t length = end != NULL ? (size_t)(end - start) : strlen(start);
    if (length >= sizeof line) {
      return BoneMap_Fail(self, ERR_BAD_ARGS, "map line too long: %.40s", start);
    }
    memcpy(line, start, length);
    line[length] = '\0';
    error = addLine(self, line);
    start = end != NULL ? end + 1 : start + length;
  }
  return error;
}

ErrorCode BoneMap_AddFile(BoneMap *self, const char *path) {
  char line[MAX_LINE_LENGTH];
  ErrorCode error = ERR_NONE;
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return BoneMap_Fail(self, ERR_OPEN_INPUT, "cannot open map file %s", path);
  }
  while (error == ERR_NONE && fgets(line, sizeof line, file) != NULL) {
    error = addLine(self, line);
  }
  fclose(file);
  return error;
}

size_t BoneMap_PairCount(const BoneMap *self) {
  return self->pair_count;
}

const char *BoneMap_SourceName(const BoneMap *self, size_t pair) {
  return self->pairs[pair].source_name;
}

const char *BoneMap_DestinationName(const BoneMap *self, size_t pair) {
  return self->pairs[pair].destination_name;
}
