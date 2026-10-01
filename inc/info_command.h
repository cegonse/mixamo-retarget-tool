#pragma once
#include <error_code.h>
#include <gltf_doc.h>
#include <stdio.h>

ErrorCode InfoCommand_Run(FILE *output, const char *path, int show_rest);
ErrorCode InfoCommand_PrintArmature(FILE *output, GltfDoc *doc, int show_rest);
