#include <bone_map.h>
#include <bone_map_data.h>
#include <stdio.h>
#include <string.h>

static void appendAvailable(BoneMap *self, const Skeleton *skeleton) {
  size_t joint, used = strlen(self->error_message);
  size_t room = sizeof self->error_message;
  for (joint = 0; joint < Skeleton_JointCount(skeleton) && used + 1 < room; joint++) {
    used += (size_t)snprintf(self->error_message + used, room - used, "%s%s",
      joint == 0 ? "; available: " : ", ", Skeleton_JointName(skeleton, joint));
  }
}

static ErrorCode resolveName(BoneMap *self, const Skeleton *skeleton, const char *name,
    const char *side, size_t *dest) {
  char format[64];
  *dest = Skeleton_FindJoint(skeleton, name);
  if (*dest != SKELETON_NO_JOINT) {
    return ERR_NONE;
  }
  snprintf(format, sizeof format, "unknown %s bone \"%%s\"", side);
  BoneMap_Fail(self, ERR_NOT_FOUND, format, name);
  appendAvailable(self, skeleton);
  return ERR_NOT_FOUND;
}

static ErrorCode resolvePair(BoneMap *self, size_t pair, const Skeleton *source,
    const Skeleton *destination) {
  BonePair *entry = &self->pairs[pair];
  ErrorCode error = resolveName(self, source, entry->source_name, "source", &entry->source_joint);
  if (error == ERR_NONE) {
    error = resolveName(self, destination, entry->destination_name, "destination",
      &entry->destination_joint);
  }
  if (error == ERR_NONE && BoneMap_PairForDestination(self, entry->destination_joint) != pair) {
    return BoneMap_Fail(self, ERR_BAD_ARGS, "destination bone \"%s\" is mapped twice",
      entry->destination_name);
  }
  return error;
}

static size_t findMappedParent(const BoneMap *self, size_t pair, const Skeleton *destination) {
  size_t joint = Skeleton_JointParent(destination, self->pairs[pair].destination_joint);
  while (joint != SKELETON_NO_JOINT) {
    size_t parent_pair = BoneMap_PairForDestination(self, joint);
    if (parent_pair != BONE_MAP_NO_PAIR) {
      return parent_pair;
    }
    joint = Skeleton_JointParent(destination, joint);
  }
  return BONE_MAP_NO_PAIR;
}

ErrorCode BoneMap_Resolve(BoneMap *self, const Skeleton *source, const Skeleton *destination) {
  size_t pair;
  ErrorCode error = ERR_NONE;
  if (self->pair_count == 0) {
    return BoneMap_Fail(self, ERR_BAD_ARGS, "bone map is empty%s", "");
  }
  for (pair = 0; pair < self->pair_count; pair++) {
    self->pairs[pair].source_joint = self->pairs[pair].destination_joint = SKELETON_NO_JOINT;
  }
  for (pair = 0; pair < self->pair_count && error == ERR_NONE; pair++) {
    error = resolvePair(self, pair, source, destination);
  }
  for (pair = 0; pair < self->pair_count && error == ERR_NONE; pair++) {
    self->pairs[pair].mapped_parent = findMappedParent(self, pair, destination);
  }
  return error;
}

size_t BoneMap_SourceJoint(const BoneMap *self, size_t pair) {
  return self->pairs[pair].source_joint;
}

size_t BoneMap_DestinationJoint(const BoneMap *self, size_t pair) {
  return self->pairs[pair].destination_joint;
}

size_t BoneMap_PairForSource(const BoneMap *self, size_t source_joint) {
  size_t pair;
  for (pair = 0; pair < self->pair_count; pair++) {
    if (self->pairs[pair].source_joint == source_joint) {
      return pair;
    }
  }
  return BONE_MAP_NO_PAIR;
}

size_t BoneMap_PairForDestination(const BoneMap *self, size_t destination_joint) {
  size_t pair;
  for (pair = 0; pair < self->pair_count; pair++) {
    if (self->pairs[pair].destination_joint == destination_joint) {
      return pair;
    }
  }
  return BONE_MAP_NO_PAIR;
}

size_t BoneMap_MappedParent(const BoneMap *self, size_t pair) {
  return self->pairs[pair].mapped_parent;
}

int BoneMap_IsRootPair(const BoneMap *self, size_t pair) {
  return self->pairs[pair].mapped_parent == BONE_MAP_NO_PAIR;
}

size_t BoneMap_MappedChildCount(const BoneMap *self, size_t pair) {
  size_t candidate, count = 0;
  for (candidate = 0; candidate < self->pair_count; candidate++) {
    count += self->pairs[candidate].mapped_parent == pair;
  }
  return count;
}

size_t BoneMap_MappedChild(const BoneMap *self, size_t pair, size_t child) {
  size_t candidate;
  for (candidate = 0; candidate < self->pair_count; candidate++) {
    if (self->pairs[candidate].mapped_parent == pair && child-- == 0) {
      return candidate;
    }
  }
  return BONE_MAP_NO_PAIR;
}
