#include <glb_build.h>
#include <stdlib.h>

static void markSubtree(GlbBuild *self, size_t node) {
  size_t child;
  if (GltfDoc_NodeHasMesh(self->doc, node)) {
    return;
  }
  self->output_index[node] = 0;
  for (child = 0; child < GltfDoc_NodeChildCount(self->doc, node); child++) {
    markSubtree(self, GltfDoc_NodeChild(self->doc, node, child));
  }
}

static void numberNodes(GlbBuild *self) {
  size_t node, next = 0, node_count = GltfDoc_NodeCount(self->doc);
  for (node = 0; node < node_count; node++) {
    self->output_index[node] = GLTF_NO_NODE;
  }
  markSubtree(self, GltfDoc_ArmatureRoot(self->doc, 0));
  for (node = 0; node < node_count; node++) {
    if (self->output_index[node] != GLTF_NO_NODE) {
      self->output_index[node] = next++;
    }
  }
  self->output_node_count = next;
}

static json_object *childrenArray(GlbBuild *self, size_t node) {
  json_object *children = json_object_new_array();
  size_t child, output;
  for (child = 0; child < GltfDoc_NodeChildCount(self->doc, node); child++) {
    output = self->output_index[GltfDoc_NodeChild(self->doc, node, child)];
    if (output != GLTF_NO_NODE) {
      json_object_array_add(children, json_object_new_int64((int64_t)output));
    }
  }
  return children;
}

static void addTrs(GlbBuild *self, json_object *object, const GltfNodeTrs *trs) {
  if (trs->has_rotation) {
    json_object_object_add(object, "rotation",
      GlbBuild_FloatArray(self, trs->transform.rotation, 4));
  }
  if (trs->has_scale) {
    json_object_object_add(object, "scale", GlbBuild_FloatArray(self, trs->transform.scale, 3));
  }
  if (trs->has_translation) {
    json_object_object_add(object, "translation",
      GlbBuild_FloatArray(self, trs->transform.translation, 3));
  }
}

static json_object *nodeObject(GlbBuild *self, size_t node) {
  json_object *object = json_object_new_object();
  json_object *children = childrenArray(self, node);
  GltfNodeTrs trs;
  if (json_object_array_length(children) > 0) {
    json_object_object_add(object, "children", children);
  } else {
    json_object_put(children);
  }
  json_object_object_add(object, "name", json_object_new_string(GltfDoc_NodeName(self->doc, node)));
  if (GltfDoc_NodeTrs(self->doc, node, &trs) != ERR_NONE) {
    self->error = ERR_BAD_GLB;
    return object;
  }
  addTrs(self, object, &trs);
  return object;
}

static void addScene(GlbBuild *self) {
  json_object *scenes = json_object_new_array();
  json_object *scene = json_object_new_object();
  json_object *scene_nodes = json_object_new_array();
  size_t root = self->output_index[GltfDoc_ArmatureRoot(self->doc, 0)];
  json_object_array_add(scene_nodes, json_object_new_int64((int64_t)root));
  json_object_object_add(scene, "name", json_object_new_string("Scene"));
  json_object_object_add(scene, "nodes", scene_nodes);
  json_object_array_add(scenes, scene);
  json_object_object_add(self->root, "scene", json_object_new_int(0));
  json_object_object_add(self->root, "scenes", scenes);
}

void GlbBuild_AddNodes(GlbBuild *self) {
  json_object *nodes = json_object_new_array();
  size_t node;
  numberNodes(self);
  addScene(self);
  for (node = 0; node < GltfDoc_NodeCount(self->doc); node++) {
    if (self->output_index[node] != GLTF_NO_NODE) {
      json_object_array_add(nodes, nodeObject(self, node));
    }
  }
  json_object_object_add(self->root, "nodes", nodes);
}

static size_t addInverseBindMatrices(GlbBuild *self) {
  size_t joint, joint_count = GltfDoc_SkinJointCount(self->doc, 0);
  float *matrices = malloc((joint_count > 0 ? joint_count : 1) * 16 * sizeof *matrices);
  size_t accessor;
  if (matrices == NULL) {
    self->error = ERR_INTERNAL;
    return 0;
  }
  for (joint = 0; joint < joint_count; joint++) {
    GltfDoc_SkinInverseBindMatrix(self->doc, 0, joint, &matrices[joint * 16]);
  }
  accessor = GlbBuild_AddAccessor(self, matrices, joint_count, "MAT4", 16, 0);
  free(matrices);
  return accessor;
}

void GlbBuild_AddSkin(GlbBuild *self) {
  json_object *skins = json_object_new_array();
  json_object *skin = json_object_new_object();
  json_object *joints = json_object_new_array();
  size_t joint;
  for (joint = 0; joint < GltfDoc_SkinJointCount(self->doc, 0); joint++) {
    size_t node = GltfDoc_SkinJoint(self->doc, 0, joint);
    json_object_array_add(joints, json_object_new_int64((int64_t)self->output_index[node]));
  }
  json_object_object_add(skin, "inverseBindMatrices",
    json_object_new_int64((int64_t)addInverseBindMatrices(self)));
  json_object_object_add(skin, "joints", joints);
  json_object_object_add(skin, "name", json_object_new_string(GltfDoc_SkinName(self->doc, 0)));
  json_object_array_add(skins, skin);
  json_object_object_add(self->root, "skins", skins);
}
