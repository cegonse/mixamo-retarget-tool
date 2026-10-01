#include <rest_align.h>
#include <transform.h>

void RestAlign_Identity(size_t pair_count, versor *corrections) {
  size_t pair;
  for (pair = 0; pair < pair_count; pair++) {
    glm_quat_identity(corrections[pair]);
  }
}

static void boneDirection(const Skeleton *skeleton, size_t joint, size_t child, vec3 dest) {
  glm_vec3_sub((float *)Skeleton_RestGlobal(skeleton, child)->translation,
    (float *)Skeleton_RestGlobal(skeleton, joint)->translation, dest);
}

static void pairCorrection(const BoneMap *map, const Skeleton *source,
    const Skeleton *destination, versor frame_rotation, size_t pair, versor dest) {
  size_t child = BoneMap_MappedChild(map, pair, 0);
  vec3 source_direction, rotated_source_direction, destination_direction;
  boneDirection(source, BoneMap_SourceJoint(map, pair), BoneMap_SourceJoint(map, child),
    source_direction);
  boneDirection(destination, BoneMap_DestinationJoint(map, pair),
    BoneMap_DestinationJoint(map, child), destination_direction);
  glm_quat_rotatev(frame_rotation, source_direction, rotated_source_direction);
  Transform_MinimalArc(rotated_source_direction, destination_direction, dest);
}

void RestAlign_Compute(const BoneMap *map, const Skeleton *source, const Skeleton *destination,
    versor frame_rotation, versor *corrections) {
  size_t joint, pair, parent;
  for (joint = 0; joint < Skeleton_JointCount(destination); joint++) {
    pair = BoneMap_PairForDestination(map, joint);
    if (pair == BONE_MAP_NO_PAIR) {
      continue;
    }
    parent = BoneMap_MappedParent(map, pair);
    if (BoneMap_MappedChildCount(map, pair) == 1) {
      pairCorrection(map, source, destination, frame_rotation, pair, corrections[pair]);
    } else if (parent != BONE_MAP_NO_PAIR) {
      glm_quat_copy(corrections[parent], corrections[pair]);
    } else {
      glm_quat_identity(corrections[pair]);
    }
  }
}
