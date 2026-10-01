#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <track_selection.h>

static size_t byIndex(GltfDoc *doc, const char *name, size_t length) {
  char digits[32];
  char *end;
  unsigned long value;
  if (length < 2 || length - 1 >= sizeof digits || name[0] != '#') {
    return GLTF_NO_NODE;
  }
  memcpy(digits, name + 1, length - 1);
  digits[length - 1] = '\0';
  value = strtoul(digits, &end, 10);
  if (*end != '\0' || digits[0] < '0' || digits[0] > '9' || value >= GltfDoc_AnimationCount(doc)) {
    return GLTF_NO_NODE;
  }
  return (size_t)value;
}

static size_t findTrack(GltfDoc *doc, const char *name, size_t length) {
  size_t animation;
  for (animation = 0; animation < GltfDoc_AnimationCount(doc); animation++) {
    const char *candidate = GltfDoc_AnimationName(doc, animation);
    if (strlen(candidate) == length && strncmp(candidate, name, length) == 0) {
      return animation;
    }
  }
  return byIndex(doc, name, length);
}

static ErrorCode reportUnknown(GltfDoc *doc, const char *name, size_t length) {
  size_t animation;
  fprintf(stderr, "error: unknown animation \"%.*s\"; available:", (int)length, name);
  for (animation = 0; animation < GltfDoc_AnimationCount(doc); animation++) {
    fprintf(stderr, "%s \"%s\" (#%zu)", animation > 0 ? "," : "",
      GltfDoc_AnimationName(doc, animation), animation);
  }
  fputc('\n', stderr);
  return ERR_NOT_FOUND;
}

static ErrorCode selectNamed(GltfDoc *doc, const char *list, size_t *indices, size_t *count) {
  const char *start = list;
  while (*count < GltfDoc_AnimationCount(doc)) {
    const char *end = strchr(start, ',');
    size_t length = end != NULL ? (size_t)(end - start) : strlen(start);
    size_t animation = findTrack(doc, start, length);
    if (animation == GLTF_NO_NODE) {
      return reportUnknown(doc, start, length);
    }
    indices[(*count)++] = animation;
    if (end == NULL) {
      return ERR_NONE;
    }
    start = end + 1;
  }
  fprintf(stderr, "error: more tracks requested than the file holds\n");
  return ERR_BAD_ARGS;
}

ErrorCode TrackSelection_Resolve(GltfDoc *doc, const Args *args, size_t *indices, size_t *count) {
  size_t animation;
  *count = 0;
  if (!args->all_animations) {
    return selectNamed(doc, args->animations, indices, count);
  }
  for (animation = 0; animation < GltfDoc_AnimationCount(doc); animation++) {
    indices[(*count)++] = animation;
  }
  return ERR_NONE;
}
