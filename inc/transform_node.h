#pragma once
#include <cgltf.h>
#include <error_code.h>
#include <transform.h>

ErrorCode Transform_FromNode(const cgltf_node *node, Transform *dest);
