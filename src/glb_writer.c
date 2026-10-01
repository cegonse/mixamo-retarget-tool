#include <file_io.h>
#include <glb_build.h>
#include <glb_container.h>
#include <glb_writer.h>
#include <stdio.h>
#include <stdlib.h>

static void addAsset(GlbBuild *self) {
  json_object *asset = json_object_new_object();
  json_object_object_add(asset, "generator", json_object_new_string(GLB_WRITER_GENERATOR));
  json_object_object_add(asset, "version", json_object_new_string("2.0"));
  json_object_object_add(self->root, "asset", asset);
}

static void addBuffers(GlbBuild *self) {
  json_object *buffers = json_object_new_array();
  json_object *buffer = json_object_new_object();
  json_object_object_add(buffer, "byteLength", json_object_new_int64((int64_t)ByteBuffer_Size(self->bin)));
  json_object_array_add(buffers, buffer);
  json_object_object_add(self->root, "accessors", self->accessors);
  json_object_object_add(self->root, "bufferViews", self->buffer_views);
  json_object_object_add(self->root, "buffers", buffers);
}

static ErrorCode startBuild(GlbBuild *self, GltfDoc *doc, const Skeleton *skeleton,
    const AnimationClip *clip) {
  self->doc = doc;
  self->skeleton = skeleton;
  self->clip = clip;
  self->error = ERR_NONE;
  self->output_index = malloc((GltfDoc_NodeCount(doc) + 1) * sizeof *self->output_index);
  self->root = json_object_new_object();
  self->accessors = json_object_new_array();
  self->buffer_views = json_object_new_array();
  self->bin = ByteBuffer_Create();
  if (self->output_index == NULL || self->root == NULL || self->accessors == NULL
      || self->buffer_views == NULL || self->bin == NULL) {
    return ERR_INTERNAL;
  }
  return ERR_NONE;
}

static void finishBuild(GlbBuild *self) {
  free(self->output_index);
  json_object_put(self->root);
  ByteBuffer_Destroy(self->bin);
}

static ByteBuffer *serialise(GlbBuild *self, ErrorCode *error) {
  size_t json_length = 0;
  const char *json = json_object_to_json_string_length(self->root,
    JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE, &json_length);
  if (json == NULL) {
    *error = ERR_INTERNAL;
    return NULL;
  }
  return GlbContainer_Frame(json, json_length, self->bin, error);
}

ByteBuffer *GlbWriter_Build(GltfDoc *destination_doc, const Skeleton *destination,
    const AnimationClip *clip, ErrorCode *error) {
  GlbBuild build = {0};
  ByteBuffer *output = NULL;
  *error = startBuild(&build, destination_doc, destination, clip);
  if (*error == ERR_NONE) {
    addAsset(&build);
    GlbBuild_AddNodes(&build);
    GlbBuild_AddAnimation(&build);
    GlbBuild_AddSkin(&build);
    addBuffers(&build);
    build.accessors = build.buffer_views = NULL;
    *error = build.error;
  }
  if (*error == ERR_NONE) {
    output = serialise(&build, error);
  }
  json_object_put(build.accessors);
  json_object_put(build.buffer_views);
  finishBuild(&build);
  return output;
}

ErrorCode GlbWriter_Write(const char *path, GltfDoc *destination_doc, const Skeleton *destination,
    const AnimationClip *clip, size_t *bytes_written) {
  ErrorCode error;
  ByteBuffer *bytes = GlbWriter_Build(destination_doc, destination, clip, &error);
  if (bytes == NULL) {
    return error;
  }
  *bytes_written = ByteBuffer_Size(bytes);
  if (FileIo_WriteBytes(path, ByteBuffer_Data(bytes), ByteBuffer_Size(bytes)) != 0) {
    fprintf(stderr, "error: cannot write %s\n", path);
    error = ERR_WRITE_OUTPUT;
  }
  ByteBuffer_Destroy(bytes);
  return error;
}
