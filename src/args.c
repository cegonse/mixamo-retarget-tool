#include <args.h>
#include <args_parse.h>
#include <stdio.h>
#include <string.h>

static int isHelp(const char *argument) {
  return strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0;
}

static ErrorCode parseInfo(int argc, char **argv, Args *dest) {
  int index;
  for (index = 2; index < argc; index++) {
    if (strcmp(argv[index], "--rest") == 0 && !dest->show_rest) {
      dest->show_rest = 1;
    } else if (argv[index][0] != '-' && dest->source_path == NULL) {
      dest->source_path = argv[index];
    } else {
      return ArgsParse_Fail("unexpected argument", argv[index]);
    }
  }
  if (dest->source_path == NULL) {
    return ArgsParse_Fail("info needs a file", "");
  }
  return ERR_NONE;
}

static ErrorCode parsePositional(Args *dest, const char *argument) {
  if (dest->source_path == NULL) {
    dest->source_path = argument;
  } else if (dest->destination_path == NULL) {
    dest->destination_path = argument;
  } else {
    return ArgsParse_Fail("unexpected argument", argument);
  }
  return ERR_NONE;
}

static ErrorCode validateConvert(const Args *dest) {
  if (dest->source_path == NULL || dest->destination_path == NULL) {
    return ArgsParse_Fail("convert needs <source.glb> <destination.glb>", "");
  }
  if ((dest->animations != NULL) == dest->all_animations) {
    return ArgsParse_Fail("give exactly one of --anim / --all-anims", "");
  }
  if (dest->map_source_count == 0) {
    return ArgsParse_Fail("give --map and/or --map-file", "");
  }
  if ((dest->out_dir != NULL) == (dest->out_file != NULL)) {
    return ArgsParse_Fail("give exactly one of --out-dir / --out", "");
  }
  return ERR_NONE;
}

static ErrorCode parseConvert(int argc, char **argv, Args *dest) {
  int index = 2;
  ErrorCode error = ERR_NONE;
  while (index < argc && error == ERR_NONE) {
    if (argv[index][0] == '-' && argv[index][1] != '\0') {
      error = ArgsParse_Flag(argc, argv, &index, dest);
    } else {
      error = parsePositional(dest, argv[index++]);
    }
  }
  return error == ERR_NONE ? validateConvert(dest) : error;
}

ErrorCode Args_Parse(int argc, char **argv, Args *dest) {
  memset(dest, 0, sizeof *dest);
  RetargetOptions_Default(&dest->retarget);
  if (argc < 2) {
    return ArgsParse_Fail("missing command", "");
  }
  if (isHelp(argv[1])) {
    dest->command = COMMAND_HELP;
    return ERR_NONE;
  }
  if (strcmp(argv[1], "info") == 0) {
    dest->command = COMMAND_INFO;
    return parseInfo(argc, argv, dest);
  }
  if (strcmp(argv[1], "convert") == 0) {
    dest->command = COMMAND_CONVERT;
    return parseConvert(argc, argv, dest);
  }
  return ArgsParse_Fail("unknown command", argv[1]);
}
