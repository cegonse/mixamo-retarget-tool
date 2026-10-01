#pragma once
#include <animation_clip.h>
#include <byte_buffer.h>
#include <frame_align.h>
#include <retarget.h>
#include <skeleton.h>
#include <track_timing.h>

typedef struct WebSession WebSession;

WebSession *WebSession_Create(void);
void WebSession_Destroy(WebSession *self);
ErrorCode WebSession_LoadSource(WebSession *self, const char *path);
ErrorCode WebSession_LoadDestination(WebSession *self, const char *path);
ErrorCode WebSession_Configure(WebSession *self, const char *map_text,
  const RetargetOptions *options, float fps);
int WebSession_IsReady(const WebSession *self);
size_t WebSession_AnimationCount(WebSession *self);
const char *WebSession_AnimationName(WebSession *self, size_t animation);
ErrorCode WebSession_AnimationTiming(WebSession *self, size_t animation, TrackTiming *dest);
char *WebSession_AnimationFileName(WebSession *self, size_t animation);
const Skeleton *WebSession_Source(const WebSession *self);
const Skeleton *WebSession_Destination(const WebSession *self);
const FrameAlignment *WebSession_Alignment(const WebSession *self);
AnimationClip *WebSession_Clip(WebSession *self, size_t animation, ErrorCode *error);
ByteBuffer *WebSession_Glb(WebSession *self, size_t animation, ErrorCode *error);
