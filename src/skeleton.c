#include <skeleton.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Joint {
  char *name;
  size_t parent;
  size_t node;
  size_t skin_index;
  Transform rest_local;
  Transform rest_global;
  Transform parent_offset;
} Joint;

struct Skeleton {
  Joint *joints;
  size_t joint_count;
  size_t capacity;
};

static size_t skinIndexOfNode(GltfDoc *doc, size_t skin, size_t node) {
  size_t joint;
  for (joint = 0; joint < GltfDoc_SkinJointCount(doc, skin); joint++) {
    if (GltfDoc_SkinJoint(doc, skin, joint) == node) {
      return joint;
    }
  }
  return SKELETON_NO_JOINT;
}

static char *copyName(const char *name) {
  size_t length = strlen(name) + 1;
  char *copy = malloc(length);
  if (copy != NULL) {
    memcpy(copy, name, length);
  }
  return copy;
}

static ErrorCode parentOffset(GltfDoc *doc, size_t skin, size_t node, Transform *dest) {
  Transform node_rest;
  size_t ancestor = GltfDoc_NodeParent(doc, node);
  ErrorCode error;
  Transform_Identity(dest);
  while (ancestor != GLTF_NO_NODE && skinIndexOfNode(doc, skin, ancestor) == SKELETON_NO_JOINT) {
    error = GltfDoc_NodeRest(doc, ancestor, &node_rest);
    if (error != ERR_NONE) {
      return error;
    }
    Transform_Compose(&node_rest, dest, dest);
    ancestor = GltfDoc_NodeParent(doc, ancestor);
  }
  return ERR_NONE;
}

static ErrorCode addJoint(Skeleton *self, GltfDoc *doc, size_t skin, size_t node, size_t parent) {
  Joint *joint = &self->joints[self->joint_count];
  ErrorCode error = GltfDoc_NodeRest(doc, node, &joint->rest_local);
  if (error == ERR_NONE) {
    error = parentOffset(doc, skin, node, &joint->parent_offset);
  }
  if (error != ERR_NONE) {
    return error;
  }
  joint->name = copyName(GltfDoc_NodeName(doc, node));
  if (joint->name == NULL) {
    return ERR_INTERNAL;
  }
  joint->parent = parent;
  joint->node = node;
  joint->skin_index = skinIndexOfNode(doc, skin, node);
  Transform_Compose(&joint->parent_offset, &joint->rest_local, &joint->rest_global);
  if (parent != SKELETON_NO_JOINT) {
    Transform_Compose(&self->joints[parent].rest_global, &joint->rest_global, &joint->rest_global);
  }
  self->joint_count++;
  return ERR_NONE;
}

static ErrorCode walkNodes(Skeleton *self, GltfDoc *doc, size_t skin, size_t node, size_t parent) {
  size_t child;
  ErrorCode error = ERR_NONE;
  if (skinIndexOfNode(doc, skin, node) != SKELETON_NO_JOINT && self->joint_count < self->capacity) {
    error = addJoint(self, doc, skin, node, parent);
    parent = self->joint_count - 1;
  }
  for (child = 0; child < GltfDoc_NodeChildCount(doc, node) && error == ERR_NONE; child++) {
    error = walkNodes(self, doc, skin, GltfDoc_NodeChild(doc, node, child), parent);
  }
  return error;
}

static ErrorCode buildJoints(Skeleton *self, GltfDoc *doc, size_t skin) {
  size_t root = GltfDoc_ArmatureRoot(doc, skin);
  ErrorCode error;
  self->capacity = GltfDoc_SkinJointCount(doc, skin);
  self->joints = calloc(self->capacity > 0 ? self->capacity : 1, sizeof *self->joints);
  if (self->joints == NULL) {
    return ERR_INTERNAL;
  }
  if (root == GLTF_NO_NODE) {
    fprintf(stderr, "error: skin %zu has no joints\n", skin);
    return ERR_BAD_GLB;
  }
  error = walkNodes(self, doc, skin, root, SKELETON_NO_JOINT);
  if (error == ERR_NONE && self->joint_count != self->capacity) {
    fprintf(stderr, "error: skin %zu joints are not all under node %zu\n", skin, root);
    return ERR_BAD_GLB;
  }
  return error;
}

Skeleton *Skeleton_FromDoc(GltfDoc *doc, size_t skin, ErrorCode *error) {
  Skeleton *self;
  if (skin >= GltfDoc_SkinCount(doc)) {
    fprintf(stderr, "error: no skin %zu (file has %zu)\n", skin, GltfDoc_SkinCount(doc));
    *error = ERR_NOT_FOUND;
    return NULL;
  }
  self = calloc(1, sizeof *self);
  if (self == NULL) {
    *error = ERR_INTERNAL;
    return NULL;
  }
  *error = buildJoints(self, doc, skin);
  if (*error != ERR_NONE) {
    Skeleton_Destroy(self);
    return NULL;
  }
  return self;
}

void Skeleton_Destroy(Skeleton *self) {
  size_t joint;
  if (self == NULL) {
    return;
  }
  for (joint = 0; joint < self->joint_count; joint++) {
    free(self->joints[joint].name);
  }
  free(self->joints);
  free(self);
}

size_t Skeleton_JointCount(const Skeleton *self) {
  return self->joint_count;
}

const char *Skeleton_JointName(const Skeleton *self, size_t joint) {
  return self->joints[joint].name;
}

size_t Skeleton_JointParent(const Skeleton *self, size_t joint) {
  return self->joints[joint].parent;
}

size_t Skeleton_JointNode(const Skeleton *self, size_t joint) {
  return self->joints[joint].node;
}

size_t Skeleton_JointSkinIndex(const Skeleton *self, size_t joint) {
  return self->joints[joint].skin_index;
}

const Transform *Skeleton_RestLocal(const Skeleton *self, size_t joint) {
  return &self->joints[joint].rest_local;
}

const Transform *Skeleton_RestGlobal(const Skeleton *self, size_t joint) {
  return &self->joints[joint].rest_global;
}

const Transform *Skeleton_ParentOffset(const Skeleton *self, size_t joint) {
  return &self->joints[joint].parent_offset;
}

size_t Skeleton_FindJoint(const Skeleton *self, const char *name) {
  size_t joint;
  for (joint = 0; joint < self->joint_count; joint++) {
    if (strcmp(self->joints[joint].name, name) == 0) {
      return joint;
    }
  }
  return SKELETON_NO_JOINT;
}

size_t Skeleton_FindJointByNode(const Skeleton *self, size_t node) {
  size_t joint;
  for (joint = 0; joint < self->joint_count; joint++) {
    if (self->joints[joint].node == node) {
      return joint;
    }
  }
  return SKELETON_NO_JOINT;
}

size_t Skeleton_ChildCount(const Skeleton *self, size_t joint) {
  size_t candidate, count = 0;
  for (candidate = joint + 1; candidate < self->joint_count; candidate++) {
    count += self->joints[candidate].parent == joint;
  }
  return count;
}

size_t Skeleton_Child(const Skeleton *self, size_t joint, size_t child) {
  size_t candidate;
  for (candidate = joint + 1; candidate < self->joint_count; candidate++) {
    if (self->joints[candidate].parent == joint && child-- == 0) {
      return candidate;
    }
  }
  return SKELETON_NO_JOINT;
}
