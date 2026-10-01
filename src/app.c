#include <app_main.h>
#include <error_code.h>
#include <string.h>

static FILE *output_stream = NULL;

static const char *const usage_text =
  "usage:\n"
  "  anim-retarget info <file.glb> [--rest]\n"
  "  anim-retarget convert <source.glb> <destination.glb>\n"
  "      (--anim <name>[,<name>...] | --all-anims)\n"
  "      (--map <src=dst>[,<src=dst>...] | --map-file <path>)\n"
  "      --out-dir <dir> | --out <file.glb>\n"
  "      [--fps <n>] [--in-place] [--src-up X|Y|Z|-X|-Y|-Z]\n"
  "      [--no-frame-align] [--frame-rotate <X,Y,Z>] [--frame-scale <k>]\n"
  "      [--no-rest-align] [--verbose]\n";

static FILE *currentOutput(void) {
  return output_stream != NULL ? output_stream : stdout;
}

static int isHelpFlag(const char *argument) {
  return strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0;
}

void App_SetOutputStream(FILE *stream) {
  output_stream = stream;
}

int App_Run(int argc, char **argv) {
  if (argc >= 2 && isHelpFlag(argv[1])) {
    fputs(usage_text, currentOutput());
    return ERR_NONE;
  }
  fputs(usage_text, stderr);
  return ERR_BAD_ARGS;
}
