#include <emscripten/emscripten.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdlib.h>
#include <string.h>
#include <web_viewer.h>
#include <web_viewer_fit.h>

enum { DEFAULT_WIDTH = 960, DEFAULT_HEIGHT = 640, GRID_SLICES = 12 };

static const float character_height = 2.0f;
static const float grid_spacing = 0.5f;
static const float joint_radius = 0.015f;
static const float pan_factor = 0.0025f;
static const Color background = {30, 32, 36, 255};
static Model model;
static int has_model = 0;
static int show_bones = 1;
static Camera3D camera;
static ModelAnimation animation;
static int has_animation = 0;
static float animation_fps = 30.0f;
static int playing = 0;
static float playhead = 0.0f;

static Vector3 bindPosition(int bone) {
  return model.skeleton.bindPose[bone].translation;
}

static void resetToBindPose(void) {
  ModelAnimation rest = {0};
  Transform *bind_pose = model.skeleton.bindPose;
  rest.boneCount = model.skeleton.boneCount;
  rest.keyframeCount = 1;
  rest.keyframePoses = &bind_pose;
  UpdateModelAnimation(model, rest, 0.0f);
}

static Vector3 shownPosition(int bone) {
  return has_animation ? model.currentPose[bone].translation : bindPosition(bone);
}

static float currentFrame(void) {
  return has_animation ? fmodf(playhead * animation_fps, (float)animation.keyframeCount) : 0.0f;
}

static void advancePlayhead(void) {
  if (has_animation && playing) {
    playhead += GetFrameTime();
    if (playhead * animation_fps >= (float)animation.keyframeCount) {
      playhead -= (float)animation.keyframeCount / animation_fps;
    }
  }
}

static void freeAnimation(void) {
  int frame;
  if (!has_animation) {
    return;
  }
  for (frame = 0; frame < animation.keyframeCount; frame++) {
    free(animation.keyframePoses[frame]);
  }
  free(animation.keyframePoses);
  has_animation = 0;
}

static void fillKeyframe(Transform *keyframe, const float *values, size_t joint_count) {
  size_t joint;
  for (joint = 0; joint < joint_count; joint++) {
    const float *slot = values + joint * 10;
    keyframe[joint].translation = (Vector3){slot[0], slot[1], slot[2]};
    keyframe[joint].rotation = (Quaternion){slot[3], slot[4], slot[5], slot[6]};
    keyframe[joint].scale = (Vector3){slot[7], slot[8], slot[9]};
  }
}

static void drawBones(void) {
  int bone;
  rlDrawRenderBatchActive();
  rlDisableDepthTest();
  for (bone = 0; bone < model.skeleton.boneCount; bone++) {
    Vector3 position = Vector3Transform(shownPosition(bone), model.transform);
    int parent = model.skeleton.bones[bone].parent;
    if (parent >= 0) {
      DrawLine3D(Vector3Transform(shownPosition(parent), model.transform), position, YELLOW);
    }
    DrawSphere(position, joint_radius, ORANGE);
  }
  rlDrawRenderBatchActive();
  rlEnableDepthTest();
}

static void updateCamera(void) {
  Vector2 delta = GetMouseDelta();
  float pan = pan_factor * Vector3Distance(camera.position, camera.target);
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    UpdateCamera(&camera, CAMERA_THIRD_PERSON);
  } else if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    UpdateCamera(&camera, CAMERA_FREE);
  } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    UpdateCameraPro(&camera, (Vector3){0.0f, -delta.x * pan, delta.y * pan}, Vector3Zero(), 0.0f);
  } else if (GetMouseWheelMove() != 0.0f) {
    UpdateCamera(&camera, CAMERA_THIRD_PERSON);
  }
}

static void drawFrame(void) {
  updateCamera();
  advancePlayhead();
  if (has_animation) {
    UpdateModelAnimation(model, animation, currentFrame());
  }
  BeginDrawing();
  ClearBackground(background);
  BeginMode3D(camera);
  DrawGrid(GRID_SLICES, grid_spacing);
  if (has_model) {
    DrawModel(model, Vector3Zero(), 1.0f, WHITE);
    if (show_bones) {
      drawBones();
    }
  }
  EndMode3D();
  EndDrawing();
}

void WebViewer_ResetCamera(void) {
  camera.position = (Vector3){0.0f, 1.6f, 4.5f};
  camera.target = (Vector3){0.0f, 1.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;
}

void WebViewer_Start(void) {
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "anim-retarget");
  WebViewer_ResetCamera();
  emscripten_set_main_loop(drawFrame, 0, 0);
}

void WebViewer_Resize(int width, int height) {
  if (width > 0 && height > 0) {
    SetWindowSize(width, height);
  }
}

int WebViewer_LoadModel(const char *path) {
  freeAnimation();
  if (has_model) {
    UnloadModel(model);
    has_model = 0;
  }
  model = LoadModel(path);
  if (model.meshCount == 0) {
    UnloadModel(model);
    return -1;
  }
  has_model = 1;
  model.transform = WebViewerFit_Matrix(&model, character_height);
  return model.skeleton.boneCount;
}

void WebViewer_SetShowBones(int show) {
  show_bones = show;
}

int WebViewer_SetAnimation(size_t frame_count, size_t joint_count, float fps,
    const float *poses) {
  size_t frame;
  freeAnimation();
  if (!has_model || frame_count == 0 || (int)joint_count != model.skeleton.boneCount) {
    return -1;
  }
  memset(&animation, 0, sizeof animation);
  animation.boneCount = (int)joint_count;
  animation.keyframeCount = (int)frame_count;
  animation.keyframePoses = calloc(frame_count, sizeof *animation.keyframePoses);
  if (animation.keyframePoses == NULL) {
    return -1;
  }
  for (frame = 0; frame < frame_count; frame++) {
    animation.keyframePoses[frame] = calloc(joint_count, sizeof **animation.keyframePoses);
    if (animation.keyframePoses[frame] == NULL) {
      animation.keyframeCount = (int)frame;
      has_animation = 1;
      freeAnimation();
      return -1;
    }
    fillKeyframe(animation.keyframePoses[frame], poses + frame * joint_count * 10, joint_count);
  }
  animation_fps = fps > 0.0f ? fps : 30.0f;
  playhead = 0.0f;
  has_animation = 1;
  return (int)frame_count;
}

void WebViewer_ClearAnimation(void) {
  freeAnimation();
  if (has_model) {
    resetToBindPose();
  }
}

void WebViewer_SetPlaying(int should_play) {
  playing = should_play && has_animation;
}

int WebViewer_IsPlaying(void) {
  return playing;
}

void WebViewer_SetFrame(int frame) {
  if (has_animation && frame >= 0 && frame < animation.keyframeCount) {
    playhead = (float)frame / animation_fps;
  }
}

int WebViewer_Frame(void) {
  return (int)currentFrame();
}

int WebViewer_FrameCount(void) {
  return has_animation ? animation.keyframeCount : 0;
}
