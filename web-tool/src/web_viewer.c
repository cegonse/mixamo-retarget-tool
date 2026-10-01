#include <emscripten/emscripten.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <web_viewer.h>

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

static Vector3 bindPosition(int bone) {
  return model.skeleton.bindPose[bone].translation;
}

static Vector3 dominantAxis(Vector3 direction) {
  float x = fabsf(direction.x), y = fabsf(direction.y), z = fabsf(direction.z);
  if (x >= y && x >= z) {
    return (Vector3){direction.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
  }
  if (z > y) {
    return (Vector3){0.0f, 0.0f, direction.z < 0.0f ? -1.0f : 1.0f};
  }
  return (Vector3){0.0f, direction.y < 0.0f ? -1.0f : 1.0f, 0.0f};
}

static int descendantCount(int bone) {
  int count = 0, other;
  for (other = 0; other < model.skeleton.boneCount; other++) {
    int ancestor = model.skeleton.bones[other].parent;
    while (ancestor >= 0 && ancestor != bone) {
      ancestor = model.skeleton.bones[ancestor].parent;
    }
    count += ancestor == bone;
  }
  return count;
}

static int rootBone(void) {
  int bone;
  for (bone = 0; bone < model.skeleton.boneCount; bone++) {
    if (model.skeleton.bones[bone].parent < 0) {
      return bone;
    }
  }
  return 0;
}

static int spineBone(int root) {
  int bone, best = -1, best_count = -1;
  for (bone = 0; bone < model.skeleton.boneCount; bone++) {
    if (model.skeleton.bones[bone].parent == root && descendantCount(bone) > best_count) {
      best = bone;
      best_count = descendantCount(bone);
    }
  }
  return best;
}

static Vector3 skeletonUp(void) {
  int root = rootBone(), spine;
  if (model.skeleton.boneCount == 0) {
    return (Vector3){0.0f, 1.0f, 0.0f};
  }
  spine = spineBone(root);
  if (spine < 0) {
    return dominantAxis(bindPosition(root));
  }
  return dominantAxis(Vector3Subtract(bindPosition(spine), bindPosition(root)));
}

static Quaternion rotationToY(Vector3 up) {
  Vector3 y = {0.0f, 1.0f, 0.0f};
  up = Vector3Normalize(up);
  if (Vector3DotProduct(up, y) < -0.9999f) {
    return QuaternionFromAxisAngle((Vector3){1.0f, 0.0f, 0.0f}, PI);
  }
  return QuaternionFromVector3ToVector3(up, y);
}

static void fitModel(void) {
  Matrix rotate = QuaternionToMatrix(rotationToY(skeletonUp()));
  float min_y = INFINITY, max_y = -INFINITY, scale;
  Vector3 sum = {0};
  int bone;
  if (model.skeleton.boneCount == 0) {
    model.transform = MatrixIdentity();
    return;
  }
  for (bone = 0; bone < model.skeleton.boneCount; bone++) {
    Vector3 position = Vector3Transform(bindPosition(bone), rotate);
    min_y = fminf(min_y, position.y);
    max_y = fmaxf(max_y, position.y);
    sum = Vector3Add(sum, position);
  }
  scale = max_y - min_y > 1e-6f ? character_height / (max_y - min_y) : 1.0f;
  sum = Vector3Scale(sum, scale / (float)model.skeleton.boneCount);
  model.transform = MatrixMultiply(MatrixMultiply(rotate, MatrixScale(scale, scale, scale)),
    MatrixTranslate(-sum.x, -min_y * scale, -sum.z));
}

static void drawBones(void) {
  int bone;
  rlDrawRenderBatchActive();
  rlDisableDepthTest();
  for (bone = 0; bone < model.skeleton.boneCount; bone++) {
    Vector3 position = Vector3Transform(bindPosition(bone), model.transform);
    int parent = model.skeleton.bones[bone].parent;
    if (parent >= 0) {
      DrawLine3D(Vector3Transform(bindPosition(parent), model.transform), position, YELLOW);
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
  fitModel();
  return model.skeleton.boneCount;
}

void WebViewer_SetShowBones(int show) {
  show_bones = show;
}
