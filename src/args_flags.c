#include <args_parse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FlagContext {
  const char *flag;
  const char *value;
  Args *args;
} FlagContext;

ErrorCode ArgsParse_Fail(const char *message, const char *argument) {
  fprintf(stderr, "error: %s%s%s\n", message, *argument ? ": " : "", argument);
  return ERR_BAD_ARGS;
}

static int parseFloat(const char *text, float *dest) {
  char *end;
  double value;
  if (*text == '\0') {
    return 0;
  }
  value = strtod(text, &end);
  *dest = (float)value;
  return *end == '\0';
}

static ErrorCode parsePositive(const FlagContext *context, int *has_value, float *dest) {
  if (!parseFloat(context->value, dest) || !(*dest > 0.0f) || *dest != *dest) {
    return ArgsParse_Fail("expected a positive number for", context->flag);
  }
  *has_value = 1;
  return ERR_NONE;
}

static ErrorCode parseTriple(const FlagContext *context, float dest[3]) {
  char buffer[128];
  char *first, *second;
  if (strlen(context->value) >= sizeof buffer) {
    return ArgsParse_Fail("value too long for", context->flag);
  }
  strcpy(buffer, context->value);
  first = strchr(buffer, ',');
  second = first != NULL ? strchr(first + 1, ',') : NULL;
  if (second == NULL || strchr(second + 1, ',') != NULL) {
    return ArgsParse_Fail("expected X,Y,Z for", context->flag);
  }
  *first = *second = '\0';
  if (!parseFloat(buffer, &dest[0]) || !parseFloat(first + 1, &dest[1])
      || !parseFloat(second + 1, &dest[2])) {
    return ArgsParse_Fail("expected X,Y,Z for", context->flag);
  }
  return ERR_NONE;
}

static ErrorCode parseUpAxis(const FlagContext *context, float dest[3]) {
  static const char *const names[] = {"X", "Y", "Z", "-X", "-Y", "-Z"};
  int axis;
  for (axis = 0; axis < 6; axis++) {
    if (strcmp(context->value, names[axis]) == 0) {
      dest[0] = dest[1] = dest[2] = 0.0f;
      dest[axis % 3] = axis < 3 ? 1.0f : -1.0f;
      return ERR_NONE;
    }
  }
  return ArgsParse_Fail("expected X|Y|Z|-X|-Y|-Z for", context->flag);
}

static ErrorCode setString(const FlagContext *context, const char **dest) {
  if (*context->value == '\0') {
    return ArgsParse_Fail("empty value for", context->flag);
  }
  *dest = context->value;
  return ERR_NONE;
}

static ErrorCode addMapSource(const FlagContext *context, int is_file) {
  Args *args = context->args;
  if (args->map_source_count == ARGS_MAX_MAP_SOURCES) {
    return ArgsParse_Fail("too many map arguments at", context->flag);
  }
  args->map_sources[args->map_source_count].is_file = is_file;
  args->map_sources[args->map_source_count].text = context->value;
  args->map_source_count++;
  return ERR_NONE;
}

static ErrorCode flagAnim(const FlagContext *context) {
  return setString(context, &context->args->animations);
}

static ErrorCode flagMap(const FlagContext *context) {
  return addMapSource(context, 0);
}

static ErrorCode flagMapFile(const FlagContext *context) {
  return addMapSource(context, 1);
}

static ErrorCode flagOutDir(const FlagContext *context) {
  return setString(context, &context->args->out_dir);
}

static ErrorCode flagOut(const FlagContext *context) {
  return setString(context, &context->args->out_file);
}

static ErrorCode flagFps(const FlagContext *context) {
  return parsePositive(context, &context->args->has_fps, &context->args->fps);
}

static ErrorCode flagSrcUp(const FlagContext *context) {
  return parseUpAxis(context, context->args->retarget.source_up);
}

static ErrorCode flagFrameRotate(const FlagContext *context) {
  context->args->retarget.has_frame_rotation = 1;
  return parseTriple(context, context->args->retarget.frame_rotation_degrees);
}

static ErrorCode flagFrameScale(const FlagContext *context) {
  RetargetOptions *retarget = &context->args->retarget;
  return parsePositive(context, &retarget->has_frame_scale, &retarget->frame_scale);
}

static ErrorCode flagAllAnims(const FlagContext *context) {
  context->args->all_animations = 1;
  return ERR_NONE;
}

static ErrorCode flagInPlace(const FlagContext *context) {
  context->args->retarget.in_place = 1;
  return ERR_NONE;
}

static ErrorCode flagNoFrameAlign(const FlagContext *context) {
  context->args->retarget.frame_align = 0;
  return ERR_NONE;
}

static ErrorCode flagNoRestAlign(const FlagContext *context) {
  context->args->retarget.rest_align = 0;
  return ERR_NONE;
}

static ErrorCode flagVerbose(const FlagContext *context) {
  context->args->verbose = 1;
  return ERR_NONE;
}

typedef struct FlagSpec {
  const char *name;
  int takes_value;
  int repeatable;
  ErrorCode (*handle)(const FlagContext *context);
} FlagSpec;

static const FlagSpec flag_specs[] = {
  {"--anim", 1, 0, flagAnim}, {"--all-anims", 0, 0, flagAllAnims},
  {"--map", 1, 1, flagMap}, {"--map-file", 1, 1, flagMapFile},
  {"--out-dir", 1, 0, flagOutDir}, {"--out", 1, 0, flagOut},
  {"--fps", 1, 0, flagFps}, {"--in-place", 0, 0, flagInPlace},
  {"--src-up", 1, 0, flagSrcUp}, {"--no-frame-align", 0, 0, flagNoFrameAlign},
  {"--frame-rotate", 1, 0, flagFrameRotate}, {"--frame-scale", 1, 0, flagFrameScale},
  {"--no-rest-align", 0, 0, flagNoRestAlign}, {"--verbose", 0, 0, flagVerbose}};

static const FlagSpec *findFlag(const char *name, size_t *position) {
  size_t index;
  for (index = 0; index < sizeof flag_specs / sizeof flag_specs[0]; index++) {
    if (strcmp(flag_specs[index].name, name) == 0) {
      *position = index;
      return &flag_specs[index];
    }
  }
  return NULL;
}

ErrorCode ArgsParse_Flag(int argc, char **argv, int *index, Args *dest) {
  size_t position = 0;
  const FlagSpec *spec = findFlag(argv[*index], &position);
  FlagContext context = {argv[*index], "", dest};
  if (spec == NULL) {
    return ArgsParse_Fail("unknown flag", argv[*index]);
  }
  if (!spec->repeatable && (dest->seen_flags & (1UL << position))) {
    return ArgsParse_Fail("duplicate flag", argv[*index]);
  }
  dest->seen_flags |= 1UL << position;
  if (spec->takes_value) {
    if (*index + 1 >= argc) {
      return ArgsParse_Fail("missing value for", argv[*index]);
    }
    context.value = argv[++*index];
  }
  (*index)++;
  return spec->handle(&context);
}
