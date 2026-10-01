#include <convert_session.h>
#include <stdio.h>
#include <string.h>

static ErrorCode loadRig(const char *path, GltfDoc **doc, Skeleton **skeleton) {
  ErrorCode error;
  *doc = GltfDoc_Load(path, &error);
  if (*doc == NULL) {
    return error;
  }
  if (GltfDoc_SkinCount(*doc) > 1) {
    fprintf(stderr, "warning: %s has %zu skins, using skin 0\n", path, GltfDoc_SkinCount(*doc));
  }
  *skeleton = Skeleton_FromDoc(*doc, 0, &error);
  return error;
}

static ErrorCode buildMap(ConvertSession *self, const Args *args) {
  size_t source;
  ErrorCode error = ERR_NONE;
  self->map = BoneMap_Create();
  if (self->map == NULL) {
    return ERR_INTERNAL;
  }
  for (source = 0; source < args->map_source_count && error == ERR_NONE; source++) {
    const MapSource *entry = &args->map_sources[source];
    error = entry->is_file ? BoneMap_AddFile(self->map, entry->text)
      : BoneMap_AddPairs(self->map, entry->text);
  }
  if (error == ERR_NONE) {
    error = BoneMap_Resolve(self->map, self->source, self->destination);
  }
  if (error != ERR_NONE) {
    fprintf(stderr, "error: %s\n", BoneMap_ErrorMessage(self->map));
  }
  return error;
}

ErrorCode ConvertSession_Open(ConvertSession *self, const Args *args) {
  ErrorCode error;
  memset(self, 0, sizeof *self);
  error = loadRig(args->source_path, &self->source_doc, &self->source);
  if (error == ERR_NONE) {
    error = loadRig(args->destination_path, &self->destination_doc, &self->destination);
  }
  if (error == ERR_NONE) {
    error = buildMap(self, args);
  }
  if (error == ERR_NONE) {
    self->retarget = Retarget_Create(self->source, self->destination, self->map, &args->retarget,
      &error);
  }
  if (error != ERR_NONE) {
    ConvertSession_Close(self);
  }
  return error;
}

void ConvertSession_Close(ConvertSession *self) {
  Retarget_Destroy(self->retarget);
  BoneMap_Destroy(self->map);
  Skeleton_Destroy(self->source);
  Skeleton_Destroy(self->destination);
  GltfDoc_Destroy(self->source_doc);
  GltfDoc_Destroy(self->destination_doc);
  memset(self, 0, sizeof *self);
}
