#include <convert_command.h>
#include <glb_writer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <track_convert.h>
#include <web_session.h>

struct WebSession {
  ConvertSession session;
  RetargetOptions options;
  float fps;
  char *map_text;
};

WebSession *WebSession_Create(void) {
  WebSession *self = calloc(1, sizeof *self);
  if (self != NULL) {
    RetargetOptions_Default(&self->options);
  }
  return self;
}

void WebSession_Destroy(WebSession *self) {
  if (self == NULL) {
    return;
  }
  ConvertSession_Close(&self->session);
  free(self->map_text);
  free(self);
}

static void dropRetarget(WebSession *self) {
  Retarget_Destroy(self->session.retarget);
  BoneMap_Destroy(self->session.map);
  self->session.retarget = NULL;
  self->session.map = NULL;
}

static ErrorCode loadRig(const char *path, GltfDoc **doc, Skeleton **skeleton) {
  ErrorCode error;
  Skeleton_Destroy(*skeleton);
  GltfDoc_Destroy(*doc);
  *skeleton = NULL;
  *doc = GltfDoc_Load(path, &error);
  if (*doc == NULL) {
    return error;
  }
  *skeleton = Skeleton_FromDoc(*doc, 0, &error);
  return error;
}

static ErrorCode buildMap(WebSession *self) {
  ErrorCode error;
  self->session.map = BoneMap_Create();
  if (self->session.map == NULL) {
    return ERR_INTERNAL;
  }
  error = BoneMap_AddText(self->session.map, self->map_text);
  if (error == ERR_NONE) {
    error = BoneMap_Resolve(self->session.map, self->session.source, self->session.destination);
  }
  if (error != ERR_NONE) {
    fprintf(stderr, "error: %s\n", BoneMap_ErrorMessage(self->session.map));
  }
  return error;
}

static ErrorCode rebuild(WebSession *self) {
  ErrorCode error;
  dropRetarget(self);
  if (self->session.source == NULL || self->session.destination == NULL
      || self->map_text == NULL) {
    return ERR_NONE;
  }
  error = buildMap(self);
  if (error == ERR_NONE) {
    self->session.retarget = Retarget_Create(self->session.source, self->session.destination,
      self->session.map, &self->options, &error);
  }
  if (error != ERR_NONE) {
    dropRetarget(self);
  }
  return error;
}

ErrorCode WebSession_LoadSource(WebSession *self, const char *path) {
  ErrorCode error;
  dropRetarget(self);
  error = loadRig(path, &self->session.source_doc, &self->session.source);
  return error == ERR_NONE ? rebuild(self) : error;
}

ErrorCode WebSession_LoadDestination(WebSession *self, const char *path) {
  ErrorCode error;
  dropRetarget(self);
  error = loadRig(path, &self->session.destination_doc, &self->session.destination);
  return error == ERR_NONE ? rebuild(self) : error;
}

ErrorCode WebSession_Configure(WebSession *self, const char *map_text,
    const RetargetOptions *options, float fps) {
  char *copy = malloc(strlen(map_text) + 1);
  if (copy == NULL) {
    return ERR_INTERNAL;
  }
  strcpy(copy, map_text);
  free(self->map_text);
  self->map_text = copy;
  self->options = *options;
  self->fps = fps;
  return rebuild(self);
}

int WebSession_IsReady(const WebSession *self) {
  return self->session.retarget != NULL;
}

size_t WebSession_AnimationCount(WebSession *self) {
  return self->session.source_doc != NULL ? GltfDoc_AnimationCount(self->session.source_doc) : 0;
}

const char *WebSession_AnimationName(WebSession *self, size_t animation) {
  return GltfDoc_AnimationName(self->session.source_doc, animation);
}

ErrorCode WebSession_AnimationTiming(WebSession *self, size_t animation, TrackTiming *dest) {
  return TrackTiming_FromDoc(self->session.source_doc, animation, dest);
}

char *WebSession_AnimationFileName(WebSession *self, size_t animation) {
  char *path = ConvertCommand_OutputPath("", WebSession_AnimationName(self, animation));
  if (path != NULL) {
    memmove(path, path + 1, strlen(path));
  }
  return path;
}

const Skeleton *WebSession_Source(const WebSession *self) {
  return self->session.source;
}

const Skeleton *WebSession_Destination(const WebSession *self) {
  return self->session.destination;
}

const FrameAlignment *WebSession_Alignment(const WebSession *self) {
  return WebSession_IsReady(self) ? Retarget_Alignment(self->session.retarget) : NULL;
}

AnimationClip *WebSession_Clip(WebSession *self, size_t animation, ErrorCode *error) {
  if (!WebSession_IsReady(self) || animation >= WebSession_AnimationCount(self)) {
    *error = ERR_NOT_FOUND;
    return NULL;
  }
  return TrackConvert_Clip(&self->session, animation, self->fps > 0.0f, self->fps, error);
}

ByteBuffer *WebSession_Glb(WebSession *self, size_t animation, ErrorCode *error) {
  AnimationClip *clip = WebSession_Clip(self, animation, error);
  ByteBuffer *bytes = NULL;
  if (clip != NULL) {
    bytes = GlbWriter_Build(self->session.destination_doc, self->session.destination, clip, error);
  }
  AnimationClip_Destroy(clip);
  return bytes;
}
