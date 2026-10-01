#include <pose.h>
#include <stdlib.h>

struct Pose {
  size_t joint_count;
  Transform *locals;
  Transform *globals;
};

Pose *Pose_Create(size_t joint_count) {
  Pose *self = calloc(1, sizeof *self);
  size_t allocated = joint_count > 0 ? joint_count : 1;
  if (self == NULL) {
    return NULL;
  }
  self->joint_count = joint_count;
  self->locals = calloc(allocated, sizeof *self->locals);
  self->globals = calloc(allocated, sizeof *self->globals);
  if (self->locals == NULL || self->globals == NULL) {
    Pose_Destroy(self);
    return NULL;
  }
  return self;
}

void Pose_Destroy(Pose *self) {
  if (self == NULL) {
    return;
  }
  free(self->locals);
  free(self->globals);
  free(self);
}

size_t Pose_JointCount(const Pose *self) {
  return self->joint_count;
}

Transform *Pose_Local(Pose *self, size_t joint) {
  return &self->locals[joint];
}

Transform *Pose_Global(Pose *self, size_t joint) {
  return &self->globals[joint];
}

void Pose_SetRest(Pose *self, const Skeleton *skeleton) {
  size_t joint;
  for (joint = 0; joint < self->joint_count; joint++) {
    self->locals[joint] = *Skeleton_RestLocal(skeleton, joint);
  }
}

void Pose_ComputeGlobals(Pose *self, const Skeleton *skeleton) {
  size_t joint, parent;
  for (joint = 0; joint < self->joint_count; joint++) {
    parent = Skeleton_JointParent(skeleton, joint);
    Transform_Compose(Skeleton_ParentOffset(skeleton, joint), &self->locals[joint],
      &self->globals[joint]);
    if (parent != SKELETON_NO_JOINT) {
      Transform_Compose(&self->globals[parent], &self->globals[joint], &self->globals[joint]);
    }
  }
}
