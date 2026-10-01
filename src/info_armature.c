#include <info_command.h>

typedef struct ArmatureWalk {
  FILE *output;
  GltfDoc *doc;
  size_t skin;
  int show_rest;
} ArmatureWalk;

static int isJoint(const ArmatureWalk *walk, size_t node) {
  size_t joint;
  for (joint = 0; joint < GltfDoc_SkinJointCount(walk->doc, walk->skin); joint++) {
    if (GltfDoc_SkinJoint(walk->doc, walk->skin, joint) == node) {
      return 1;
    }
  }
  return 0;
}

static void printRest(FILE *output, const Transform *rest) {
  fprintf(output, "  T=(%.4f, %.4f, %.4f) R=(%.4f, %.4f, %.4f, %.4f) S=(%.4f, %.4f, %.4f)",
    rest->translation[0], rest->translation[1], rest->translation[2], rest->rotation[0],
    rest->rotation[1], rest->rotation[2], rest->rotation[3], rest->scale[0], rest->scale[1],
    rest->scale[2]);
}

static ErrorCode printJoint(const ArmatureWalk *walk, size_t node, size_t depth) {
  Transform rest;
  ErrorCode error = ERR_NONE;
  fprintf(walk->output, "%*s%s", (int)(2 * depth + 2), "", GltfDoc_NodeName(walk->doc, node));
  if (walk->show_rest) {
    error = GltfDoc_NodeRest(walk->doc, node, &rest);
    if (error == ERR_NONE) {
      printRest(walk->output, &rest);
    }
  }
  fputc('\n', walk->output);
  return error;
}

static ErrorCode walkNode(const ArmatureWalk *walk, size_t node, size_t depth) {
  size_t child;
  ErrorCode error = ERR_NONE;
  if (isJoint(walk, node)) {
    error = printJoint(walk, node, depth);
    depth++;
  }
  for (child = 0; child < GltfDoc_NodeChildCount(walk->doc, node) && error == ERR_NONE; child++) {
    error = walkNode(walk, GltfDoc_NodeChild(walk->doc, node, child), depth);
  }
  return error;
}

ErrorCode InfoCommand_PrintArmature(FILE *output, GltfDoc *doc, int show_rest) {
  ArmatureWalk walk = {output, doc, 0, show_rest};
  size_t root;
  if (GltfDoc_SkinCount(doc) == 0) {
    return ERR_NONE;
  }
  root = GltfDoc_ArmatureRoot(doc, 0);
  if (root == GLTF_NO_NODE) {
    return ERR_NONE;
  }
  if (GltfDoc_SkinCount(doc) > 1) {
    fprintf(stderr, "warning: %zu skins, using skin 0\n", GltfDoc_SkinCount(doc));
  }
  fprintf(output, "armature (skin 0, root node %zu \"%s\"):\n", root, GltfDoc_NodeName(doc, root));
  return walkNode(&walk, root, 0);
}
