#pragma once
#include <error_code.h>
#include <retarget.h>
#include <stddef.h>

typedef enum Command {
  COMMAND_HELP,
  COMMAND_INFO,
  COMMAND_CONVERT
} Command;

typedef struct MapSource {
  int is_file;
  const char *text;
} MapSource;

enum { ARGS_MAX_MAP_SOURCES = 64 };

typedef struct Args {
  Command command;
  const char *source_path;
  const char *destination_path;
  int show_rest;
  const char *animations;
  int all_animations;
  MapSource map_sources[ARGS_MAX_MAP_SOURCES];
  size_t map_source_count;
  const char *out_dir;
  const char *out_file;
  int has_fps;
  float fps;
  RetargetOptions retarget;
  int verbose;
  unsigned long seen_flags;
} Args;

ErrorCode Args_Parse(int argc, char **argv, Args *dest);
