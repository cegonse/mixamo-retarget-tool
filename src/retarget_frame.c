#include <math.h>
#include <stdio.h>
#include <retarget.h>
#include <retarget_data.h>

static void parentGlobal(Retarget *self, Pose *destination_pose, size_t joint, Transform *dest) {
  size_t parent = Skeleton_JointParent(self->destination, joint);
  *dest = *Skeleton_ParentOffset(self->destination, joint);
  if (parent != SKELETON_NO_JOINT) {
    Transform_Compose(Pose_Global(destination_pose, parent), dest, dest);
  }
}

static void globalRotation(Retarget *self, Pose *source_pose, size_t pair, versor dest) {
  size_t source_joint = BoneMap_SourceJoint(self->map, pair);
  size_t destination_joint = BoneMap_DestinationJoint(self->map, pair);
  versor rest_inverse, delta, half_aligned, aligned, frame_inverse, correction_inverse, corrected;
  glm_quat_inv((float *)Skeleton_RestGlobal(self->source, source_joint)->rotation, rest_inverse);
  glm_quat_mul(Pose_Global(source_pose, source_joint)->rotation, rest_inverse, delta);
  glm_quat_inv(self->alignment.rotation, frame_inverse);
  glm_quat_mul(self->alignment.rotation, delta, half_aligned);
  glm_quat_mul(half_aligned, frame_inverse, aligned);
  glm_quat_inv(self->corrections[pair], correction_inverse);
  glm_quat_mul(aligned, correction_inverse, corrected);
  glm_quat_mul(corrected,
    (float *)Skeleton_RestGlobal(self->destination, destination_joint)->rotation, dest);
  glm_quat_normalize(dest);
}

static void stripHorizontal(Retarget *self, vec3 displacement) {
  float vertical = glm_vec3_dot(displacement, self->up);
  glm_vec3_scale(self->up, vertical, displacement);
}

static void rootPosition(Retarget *self, Pose *source_pose, size_t pair, vec3 dest) {
  size_t source_joint = BoneMap_SourceJoint(self->map, pair);
  size_t destination_joint = BoneMap_DestinationJoint(self->map, pair);
  vec3 displacement;
  glm_vec3_sub(Pose_Global(source_pose, source_joint)->translation,
    (float *)Skeleton_RestGlobal(self->source, source_joint)->translation, displacement);
  glm_quat_rotatev(self->alignment.rotation, displacement, displacement);
  glm_vec3_scale(displacement, self->alignment.scale, displacement);
  if (self->in_place) {
    stripHorizontal(self, displacement);
  }
  glm_vec3_add((float *)Skeleton_RestGlobal(self->destination, destination_joint)->translation,
    displacement, dest);
}

static void retargetJoint(Retarget *self, Pose *source_pose, Pose *destination_pose,
    size_t joint) {
  size_t pair = BoneMap_PairForDestination(self->map, joint);
  Transform parent, parent_inverse, *local = Pose_Local(destination_pose, joint);
  versor rotation, parent_rotation_inverse;
  vec3 position;
  *local = *Skeleton_RestLocal(self->destination, joint);
  parentGlobal(self, destination_pose, joint, &parent);
  if (pair != BONE_MAP_NO_PAIR) {
    globalRotation(self, source_pose, pair, rotation);
    glm_quat_inv(parent.rotation, parent_rotation_inverse);
    glm_quat_mul(parent_rotation_inverse, rotation, local->rotation);
    glm_quat_normalize(local->rotation);
    if (BoneMap_IsRootPair(self->map, pair)) {
      rootPosition(self, source_pose, pair, position);
      Transform_Inverse(&parent, &parent_inverse);
      Transform_TransformPoint(&parent_inverse, position, local->translation);
    }
  }
  Transform_Compose(&parent, local, Pose_Global(destination_pose, joint));
}

static int isFinite(const Transform *transform) {
  int index;
  for (index = 0; index < 3; index++) {
    if (!isfinite(transform->translation[index]) || !isfinite(transform->scale[index])) {
      return 0;
    }
  }
  for (index = 0; index < 4; index++) {
    if (!isfinite(transform->rotation[index])) {
      return 0;
    }
  }
  return 1;
}

static void makeContinuous(Retarget *self, Pose *destination_pose, size_t joint) {
  Transform *local = Pose_Local(destination_pose, joint);
  if (self->has_previous) {
    Transform_QuatMakeContinuous(self->previous_rotations[joint], local->rotation);
  }
  glm_quat_copy(local->rotation, self->previous_rotations[joint]);
}

ErrorCode Retarget_Frame(Retarget *self, Pose *source_pose, Pose *destination_pose) {
  size_t joint, joint_count = Skeleton_JointCount(self->destination);
  Pose_ComputeGlobals(source_pose, self->source);
  for (joint = 0; joint < joint_count; joint++) {
    retargetJoint(self, source_pose, destination_pose, joint);
    if (!isFinite(Pose_Local(destination_pose, joint))) {
      fprintf(stderr, "error: non-finite result on joint \"%s\"\n",
        Skeleton_JointName(self->destination, joint));
      return ERR_INTERNAL;
    }
  }
  for (joint = 0; joint < joint_count; joint++) {
    makeContinuous(self, destination_pose, joint);
  }
  self->has_previous = 1;
  return ERR_NONE;
}
