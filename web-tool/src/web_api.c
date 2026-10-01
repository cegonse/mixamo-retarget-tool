#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <transform.h>
#include <web_pose.h>
#include <web_session.h>
#include <web_viewer.h>

static WebSession *session = NULL;
static ByteBuffer *exported = NULL;
static char alignment_text[160];

static WebSession *currentSession(void) {
  if (session == NULL) {
    session = WebSession_Create();
  }
  return session;
}

EMSCRIPTEN_KEEPALIVE int web_load_source(const char *path) {
  return (int)WebSession_LoadSource(currentSession(), path);
}

EMSCRIPTEN_KEEPALIVE int web_load_destination(const char *path) {
  ErrorCode error = WebSession_LoadDestination(currentSession(), path);
  if (error == ERR_NONE && WebViewer_LoadModel(path) < 0) {
    fprintf(stderr, "warning: the destination has no mesh to display\n");
  }
  return (int)error;
}

EMSCRIPTEN_KEEPALIVE int web_configure(const char *map_text, int frame_align, int rest_align,
    int in_place, float fps) {
  RetargetOptions options;
  RetargetOptions_Default(&options);
  options.frame_align = frame_align;
  options.rest_align = rest_align;
  options.in_place = in_place;
  return (int)WebSession_Configure(currentSession(), map_text, &options, fps);
}

EMSCRIPTEN_KEEPALIVE int web_is_ready(void) {
  return WebSession_IsReady(currentSession());
}

EMSCRIPTEN_KEEPALIVE int web_animation_count(void) {
  return (int)WebSession_AnimationCount(currentSession());
}

EMSCRIPTEN_KEEPALIVE const char *web_animation_name(int animation) {
  return WebSession_AnimationName(currentSession(), (size_t)animation);
}

EMSCRIPTEN_KEEPALIVE float web_animation_duration(int animation) {
  TrackTiming timing;
  if (WebSession_AnimationTiming(currentSession(), (size_t)animation, &timing) != ERR_NONE) {
    return 0.0f;
  }
  return timing.end - timing.start;
}

EMSCRIPTEN_KEEPALIVE char *web_animation_file_name(int animation) {
  return WebSession_AnimationFileName(currentSession(), (size_t)animation);
}

EMSCRIPTEN_KEEPALIVE void web_free(void *pointer) {
  free(pointer);
}

EMSCRIPTEN_KEEPALIVE const char *web_alignment_text(void) {
  const FrameAlignment *alignment = WebSession_Alignment(currentSession());
  vec3 degrees;
  if (alignment == NULL) {
    return "";
  }
  Transform_EulerDegrees((float *)alignment->rotation, degrees);
  snprintf(alignment_text, sizeof alignment_text,
    "frame: rotate (%.1f°, %.1f°, %.1f°)  scale %.2f  rms %.1f (%.0f%% of spread)%s",
    degrees[0], degrees[1], degrees[2], alignment->scale, alignment->rms_residual,
    alignment->destination_spread > 0.0f
      ? 100.0f * alignment->rms_residual / alignment->destination_spread : 0.0f,
    alignment->degenerate ? "  [degenerate: identity used]" : "");
  return alignment_text;
}

EMSCRIPTEN_KEEPALIVE int web_build_glb(int animation) {
  ErrorCode error = ERR_NONE;
  ByteBuffer_Destroy(exported);
  exported = WebSession_Glb(currentSession(), (size_t)animation, &error);
  return exported != NULL ? (int)ByteBuffer_Size(exported) : -(int)error;
}

EMSCRIPTEN_KEEPALIVE const uint8_t *web_glb_data(void) {
  return exported != NULL ? ByteBuffer_Data(exported) : NULL;
}

EMSCRIPTEN_KEEPALIVE void web_glb_release(void) {
  ByteBuffer_Destroy(exported);
  exported = NULL;
}

EMSCRIPTEN_KEEPALIVE void web_resize(int width, int height) {
  WebViewer_Resize(width, height);
}

EMSCRIPTEN_KEEPALIVE void web_set_show_bones(int show) {
  WebViewer_SetShowBones(show);
}

EMSCRIPTEN_KEEPALIVE void web_reset_camera(void) {
  WebViewer_ResetCamera();
}

static int preview_animation = -1;

static int showPreview(void) {
  ErrorCode error = ERR_NONE;
  AnimationClip *clip;
  WebPoseSet *poses;
  int frames = -1;
  if (preview_animation < 0 || !WebSession_IsReady(currentSession())) {
    WebViewer_ClearAnimation();
    return 0;
  }
  clip = WebSession_Clip(currentSession(), (size_t)preview_animation, &error);
  poses = clip != NULL ? WebPoseSet_FromClip(clip, WebSession_Destination(currentSession())) : NULL;
  if (poses != NULL) {
    frames = WebViewer_SetAnimation(WebPoseSet_FrameCount(poses), WebPoseSet_JointCount(poses),
      WebPoseSet_Fps(poses), WebPoseSet_Frame(poses, 0));
  }
  WebPoseSet_Destroy(poses);
  AnimationClip_Destroy(clip);
  return frames >= 0 ? frames : -(int)(error != ERR_NONE ? error : ERR_INTERNAL);
}

EMSCRIPTEN_KEEPALIVE int web_preview(int animation) {
  int frames;
  preview_animation = animation;
  frames = showPreview();
  WebViewer_SetPlaying(frames > 0);
  return frames;
}

EMSCRIPTEN_KEEPALIVE int web_refresh_preview(void) {
  return showPreview();
}

EMSCRIPTEN_KEEPALIVE void web_set_playing(int playing) {
  WebViewer_SetPlaying(playing);
}

EMSCRIPTEN_KEEPALIVE int web_is_playing(void) {
  return WebViewer_IsPlaying();
}

EMSCRIPTEN_KEEPALIVE void web_set_frame(int frame) {
  WebViewer_SetFrame(frame);
}

EMSCRIPTEN_KEEPALIVE int web_frame(void) {
  return WebViewer_Frame();
}

EMSCRIPTEN_KEEPALIVE int web_frame_count(void) {
  return WebViewer_FrameCount();
}
