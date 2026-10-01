#include <math.h>
#include <web_viewer_fit.h>

static Vector3 bonePosition(const Model *model, int bone) {
  return model->skeleton.bindPose[bone].translation;
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

static int descendantCount(const Model *model, int bone) {
  int count = 0, other;
  for (other = 0; other < model->skeleton.boneCount; other++) {
    int ancestor = model->skeleton.bones[other].parent;
    while (ancestor >= 0 && ancestor != bone) {
      ancestor = model->skeleton.bones[ancestor].parent;
    }
    count += ancestor == bone;
  }
  return count;
}

static int rootBone(const Model *model) {
  int bone;
  for (bone = 0; bone < model->skeleton.boneCount; bone++) {
    if (model->skeleton.bones[bone].parent < 0) {
      return bone;
    }
  }
  return 0;
}

static int spineBone(const Model *model, int root) {
  int bone, best = -1, best_count = -1;
  for (bone = 0; bone < model->skeleton.boneCount; bone++) {
    if (model->skeleton.bones[bone].parent == root && descendantCount(model, bone) > best_count) {
      best = bone;
      best_count = descendantCount(model, bone);
    }
  }
  return best;
}

static Vector3 skeletonUp(const Model *model) {
  int root = rootBone(model), spine;
  if (model->skeleton.boneCount == 0) {
    return (Vector3){0.0f, 1.0f, 0.0f};
  }
  spine = spineBone(model, root);
  if (spine < 0) {
    return dominantAxis(bonePosition(model, root));
  }
  return dominantAxis(Vector3Subtract(bonePosition(model, spine), bonePosition(model, root)));
}

static Quaternion rotationToY(Vector3 up) {
  Vector3 y = {0.0f, 1.0f, 0.0f};
  up = Vector3Normalize(up);
  if (Vector3DotProduct(up, y) < -0.9999f) {
    return QuaternionFromAxisAngle((Vector3){1.0f, 0.0f, 0.0f}, PI);
  }
  return QuaternionFromVector3ToVector3(up, y);
}

Matrix WebViewerFit_Matrix(const Model *model, float character_height) {
  Matrix rotate = QuaternionToMatrix(rotationToY(skeletonUp(model)));
  float min_y = INFINITY, max_y = -INFINITY, scale;
  Vector3 sum = {0};
  int bone;
  if (model->skeleton.boneCount == 0) {
    return MatrixIdentity();
  }
  for (bone = 0; bone < model->skeleton.boneCount; bone++) {
    Vector3 position = Vector3Transform(bonePosition(model, bone), rotate);
    min_y = fminf(min_y, position.y);
    max_y = fmaxf(max_y, position.y);
    sum = Vector3Add(sum, position);
  }
  scale = max_y - min_y > 1e-6f ? character_height / (max_y - min_y) : 1.0f;
  sum = Vector3Scale(sum, scale / (float)model->skeleton.boneCount);
  return MatrixMultiply(MatrixMultiply(rotate, MatrixScale(scale, scale, scale)),
    MatrixTranslate(-sum.x, -min_y * scale, -sum.z));
}
