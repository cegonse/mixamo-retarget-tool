#pragma once
#include <error_code.h>
#include <skeleton.h>
#include <stddef.h>

typedef struct BoneMap BoneMap;

#define BONE_MAP_NO_PAIR ((size_t)-1)

BoneMap *BoneMap_Create(void);
void BoneMap_Destroy(BoneMap *self);
ErrorCode BoneMap_AddPairs(BoneMap *self, const char *text);
ErrorCode BoneMap_AddFile(BoneMap *self, const char *path);
const char *BoneMap_ErrorMessage(const BoneMap *self);
size_t BoneMap_PairCount(const BoneMap *self);
const char *BoneMap_SourceName(const BoneMap *self, size_t pair);
const char *BoneMap_DestinationName(const BoneMap *self, size_t pair);

ErrorCode BoneMap_Resolve(BoneMap *self, const Skeleton *source, const Skeleton *destination);
size_t BoneMap_SourceJoint(const BoneMap *self, size_t pair);
size_t BoneMap_DestinationJoint(const BoneMap *self, size_t pair);
size_t BoneMap_PairForSource(const BoneMap *self, size_t source_joint);
size_t BoneMap_PairForDestination(const BoneMap *self, size_t destination_joint);
size_t BoneMap_MappedParent(const BoneMap *self, size_t pair);
int BoneMap_IsRootPair(const BoneMap *self, size_t pair);
size_t BoneMap_MappedChildCount(const BoneMap *self, size_t pair);
size_t BoneMap_MappedChild(const BoneMap *self, size_t pair, size_t child);
