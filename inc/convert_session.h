#pragma once
#include <args.h>
#include <bone_map.h>
#include <gltf_doc.h>
#include <retarget.h>
#include <skeleton.h>

typedef struct ConvertSession {
  GltfDoc *source_doc;
  GltfDoc *destination_doc;
  Skeleton *source;
  Skeleton *destination;
  BoneMap *map;
  Retarget *retarget;
} ConvertSession;

ErrorCode ConvertSession_Open(ConvertSession *self, const Args *args);
void ConvertSession_Close(ConvertSession *self);
