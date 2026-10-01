#pragma once
#include <gltf_doc.h>
#include <transform.h>

typedef struct Skeleton Skeleton;

#define SKELETON_NO_JOINT ((size_t)-1)

Skeleton *Skeleton_FromDoc(GltfDoc *doc, size_t skin, ErrorCode *error);
void Skeleton_Destroy(Skeleton *self);
size_t Skeleton_JointCount(const Skeleton *self);
const char *Skeleton_JointName(const Skeleton *self, size_t joint);
size_t Skeleton_JointParent(const Skeleton *self, size_t joint);
size_t Skeleton_JointNode(const Skeleton *self, size_t joint);
size_t Skeleton_JointSkinIndex(const Skeleton *self, size_t joint);
const Transform *Skeleton_RestLocal(const Skeleton *self, size_t joint);
const Transform *Skeleton_RestGlobal(const Skeleton *self, size_t joint);
const Transform *Skeleton_ParentOffset(const Skeleton *self, size_t joint);
size_t Skeleton_FindJoint(const Skeleton *self, const char *name);
size_t Skeleton_FindJointByNode(const Skeleton *self, size_t node);
size_t Skeleton_ChildCount(const Skeleton *self, size_t joint);
size_t Skeleton_Child(const Skeleton *self, size_t joint, size_t child);
