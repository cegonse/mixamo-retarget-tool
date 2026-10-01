#pragma once
#include <args.h>
#include <stdio.h>

ErrorCode ConvertCommand_Run(FILE *output, const Args *args);
char *ConvertCommand_OutputPath(const char *out_dir, const char *track_name);
