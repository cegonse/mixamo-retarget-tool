#pragma once
#include <args.h>

ErrorCode ArgsParse_Fail(const char *message, const char *argument);
ErrorCode ArgsParse_Flag(int argc, char **argv, int *index, Args *dest);
