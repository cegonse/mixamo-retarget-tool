#include <transform_node.h>
#include <string.h>

static void copyTrs(const cgltf_node *node, Transform *dest) {
  Transform_Identity(dest);
  if (node->has_translation) {
    memcpy(dest->translation, node->translation, sizeof dest->translation);
  }
  if (node->has_rotation) {
    memcpy(dest->rotation, node->rotation, sizeof dest->rotation);
  }
  if (node->has_scale) {
    memcpy(dest->scale, node->scale, sizeof dest->scale);
  }
}

ErrorCode Transform_FromNode(const cgltf_node *node, Transform *dest) {
  mat4 matrix;
  if (!node->has_matrix) {
    copyTrs(node, dest);
    return ERR_NONE;
  }
  memcpy(matrix, node->matrix, sizeof matrix);
  return Transform_FromMat4(matrix, dest);
}
