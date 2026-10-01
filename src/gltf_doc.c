#include <gltf_doc.h>
#include <gltf_doc_data.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <transform_node.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

struct GltfDoc {
  cgltf_data *data;
};

static const char *resultName(cgltf_result result) {
  static const char *const names[] = {
    "cgltf_result_success", "cgltf_result_data_too_short", "cgltf_result_unknown_format",
    "cgltf_result_invalid_json", "cgltf_result_invalid_gltf", "cgltf_result_invalid_options",
    "cgltf_result_file_not_found", "cgltf_result_io_error", "cgltf_result_out_of_memory",
    "cgltf_result_legacy_gltf"};
  if ((size_t)result < sizeof names / sizeof names[0]) {
    return names[result];
  }
  return "cgltf_result_unknown";
}

static ErrorCode errorFromResult(cgltf_result result) {
  if (result == cgltf_result_file_not_found || result == cgltf_result_io_error) {
    return ERR_OPEN_INPUT;
  }
  return ERR_BAD_GLB;
}

static cgltf_result parseAndValidate(const char *path, cgltf_data **data) {
  cgltf_options options = {0};
  cgltf_result result = cgltf_parse_file(&options, path, data);
  if (result == cgltf_result_success) {
    result = cgltf_load_buffers(&options, *data, path);
  }
  if (result == cgltf_result_success) {
    result = cgltf_validate(*data);
  }
  return result;
}

GltfDoc *GltfDoc_Load(const char *path, ErrorCode *error) {
  cgltf_data *data = NULL;
  cgltf_result result = parseAndValidate(path, &data);
  GltfDoc *self;
  if (result != cgltf_result_success) {
    fprintf(stderr, "error: cannot load %s: %s\n", path, resultName(result));
    cgltf_free(data);
    *error = errorFromResult(result);
    return NULL;
  }
  self = malloc(sizeof *self);
  if (self == NULL) {
    cgltf_free(data);
    *error = ERR_INTERNAL;
    return NULL;
  }
  self->data = data;
  *error = ERR_NONE;
  return self;
}

void GltfDoc_Destroy(GltfDoc *self) {
  if (self == NULL) {
    return;
  }
  cgltf_free(self->data);
  free(self);
}

cgltf_data *GltfDoc_Data(GltfDoc *self) {
  return self->data;
}

static const char *orEmpty(const char *text) {
  return text != NULL ? text : "";
}

const char *GltfDoc_Version(GltfDoc *self) {
  return orEmpty(self->data->asset.version);
}

const char *GltfDoc_Generator(GltfDoc *self) {
  return orEmpty(self->data->asset.generator);
}

static cgltf_node *nodeAt(GltfDoc *self, size_t node) {
  return &self->data->nodes[node];
}

static size_t indexOfNode(GltfDoc *self, const cgltf_node *node) {
  return node != NULL ? cgltf_node_index(self->data, node) : GLTF_NO_NODE;
}

size_t GltfDoc_NodeCount(GltfDoc *self) {
  return self->data->nodes_count;
}

const char *GltfDoc_NodeName(GltfDoc *self, size_t node) {
  return orEmpty(nodeAt(self, node)->name);
}

size_t GltfDoc_NodeParent(GltfDoc *self, size_t node) {
  return indexOfNode(self, nodeAt(self, node)->parent);
}

size_t GltfDoc_NodeChildCount(GltfDoc *self, size_t node) {
  return nodeAt(self, node)->children_count;
}

size_t GltfDoc_NodeChild(GltfDoc *self, size_t node, size_t child) {
  return indexOfNode(self, nodeAt(self, node)->children[child]);
}

int GltfDoc_NodeHasMesh(GltfDoc *self, size_t node) {
  return nodeAt(self, node)->mesh != NULL;
}

int GltfDoc_NodeHasSkin(GltfDoc *self, size_t node) {
  return nodeAt(self, node)->skin != NULL;
}

ErrorCode GltfDoc_NodeRest(GltfDoc *self, size_t node, Transform *dest) {
  ErrorCode error = Transform_FromNode(nodeAt(self, node), dest);
  if (error != ERR_NONE) {
    fprintf(stderr, "error: node %zu \"%s\" has a sheared or degenerate matrix\n", node,
      GltfDoc_NodeName(self, node));
  }
  return error;
}

void GltfDoc_NodeRestWorldMatrix(GltfDoc *self, size_t node, mat4 dest) {
  cgltf_node_transform_world(nodeAt(self, node), (float *)dest);
}

static cgltf_skin *skinAt(GltfDoc *self, size_t skin) {
  return &self->data->skins[skin];
}

size_t GltfDoc_SkinCount(GltfDoc *self) {
  return self->data->skins_count;
}

const char *GltfDoc_SkinName(GltfDoc *self, size_t skin) {
  return orEmpty(skinAt(self, skin)->name);
}

size_t GltfDoc_SkinJointCount(GltfDoc *self, size_t skin) {
  return skinAt(self, skin)->joints_count;
}

size_t GltfDoc_SkinJoint(GltfDoc *self, size_t skin, size_t joint) {
  return indexOfNode(self, skinAt(self, skin)->joints[joint]);
}

int GltfDoc_SkinHasInverseBindMatrices(GltfDoc *self, size_t skin) {
  return skinAt(self, skin)->inverse_bind_matrices != NULL;
}

size_t GltfDoc_SkinInverseBindAccessor(GltfDoc *self, size_t skin) {
  cgltf_accessor *accessor = skinAt(self, skin)->inverse_bind_matrices;
  return accessor != NULL ? cgltf_accessor_index(self->data, accessor) : GLTF_NO_NODE;
}

void GltfDoc_SkinInverseBindMatrix(GltfDoc *self, size_t skin, size_t joint, float dest[16]) {
  cgltf_accessor *accessor = skinAt(self, skin)->inverse_bind_matrices;
  mat4 identity = GLM_MAT4_IDENTITY_INIT;
  if (accessor == NULL || !cgltf_accessor_read_float(accessor, joint, dest, 16)) {
    memcpy(dest, identity, sizeof identity);
  }
}

size_t GltfDoc_ArmatureRoot(GltfDoc *self, size_t skin) {
  cgltf_node *node;
  if (skinAt(self, skin)->joints_count == 0) {
    return GLTF_NO_NODE;
  }
  node = skinAt(self, skin)->joints[0];
  while (node->parent != NULL) {
    node = node->parent;
  }
  return indexOfNode(self, node);
}
