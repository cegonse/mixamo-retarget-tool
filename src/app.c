#include <app_main.h>
#include <args.h>
#include <convert_command.h>
#include <error_code.h>
#include <info_command.h>

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

void App_SetOutputStream(FILE *stream) {
  output_stream = stream;
}

int App_Run(int argc, char **argv) {
  Args args;
  ErrorCode error = Args_Parse(argc, argv, &args);
  if (error != ERR_NONE) {
    fputs(usage_text, stderr);
    return error;
  }
  if (args.command == COMMAND_HELP) {
    fputs(usage_text, currentOutput());
    return ERR_NONE;
  }
  if (args.command == COMMAND_INFO) {
    return InfoCommand_Run(currentOutput(), args.source_path, args.show_rest);
  }
  return ConvertCommand_Run(currentOutput(), &args);
}
