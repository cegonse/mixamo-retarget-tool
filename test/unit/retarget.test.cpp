#include <cest>
#include <cmath>
#include <vector>
extern "C" {
#include <anim_track.h>
#include <retarget.h>
}

static GltfDoc *source_doc = nullptr, *destination_doc = nullptr;
static Skeleton *source = nullptr, *destination = nullptr;
static BoneMap *map = nullptr;
static AnimTrack *track = nullptr;
static Retarget *retarget = nullptr;
static Pose *source_pose = nullptr, *destination_pose = nullptr;

static void loadPair(const char *source_path, const char *destination_path, const char *map_path,
    size_t animation) {
  ErrorCode error;
  source_doc = GltfDoc_Load(source_path, &error);
  destination_doc = GltfDoc_Load(destination_path, &error);
  source = Skeleton_FromDoc(source_doc, 0, &error);
  destination = Skeleton_FromDoc(destination_doc, 0, &error);
  track = AnimTrack_FromDoc(source_doc, animation, source, &error);
  map = BoneMap_Create();
  BoneMap_AddFile(map, map_path);
  expect((int)BoneMap_Resolve(map, source, destination)).toBe((int)ERR_NONE);
  source_pose = Pose_Create(Skeleton_JointCount(source));
  destination_pose = Pose_Create(Skeleton_JointCount(destination));
}

static void loadSwordRun() {
  loadPair(FIXTURES_DIR "/sword_run.glb", FIXTURES_DIR "/sword_run.glb",
    DOCS_DIR "/mappings/mixamo-identity.map", 0);
}

static void loadJog() {
  loadPair(FIXTURES_DIR "/UAL1_Standard_RM.glb", FIXTURES_DIR "/test_player.glb",
    DOCS_DIR "/mappings/ual-to-mixamo.map", 13);
}

static void createRetarget(const RetargetOptions *options) {
  ErrorCode error;
  retarget = Retarget_Create(source, destination, map, options, &error);
  expect((int)error).toBe((int)ERR_NONE);
}

static void createDefaultRetarget() {
  RetargetOptions options;
  RetargetOptions_Default(&options);
  createRetarget(&options);
}

static float frameTime(size_t frame) {
  return AnimTrack_Start(track) + (float)frame / AnimTrack_Fps(track);
}

static void runFrame(size_t frame) {
  AnimTrack_EvaluatePose(track, frameTime(frame), source_pose);
  expect((int)Retarget_Frame(retarget, source_pose, destination_pose)).toBe((int)ERR_NONE);
}

static std::vector<Transform> runTrack() {
  std::vector<Transform> locals;
  for (size_t frame = 0; frame < AnimTrack_FrameCount(track, AnimTrack_Fps(track)); frame++) {
    runFrame(frame);
    for (size_t joint = 0; joint < Skeleton_JointCount(destination); joint++) {
      locals.push_back(*Pose_Local(destination_pose, joint));
    }
  }
  return locals;
}

static void expectSameRotation(const float *actual, const float *expected, float epsilon) {
  expect(fabsf(glm_quat_dot((float *)actual, (float *)expected))).toBe(1.0f, epsilon);
}

static void expectSameLocals(const std::vector<Transform> &actual, const std::vector<Transform> &expected) {
  expect(actual.size()).toBe(expected.size());
  for (size_t index = 0; index < actual.size(); index++) {
    expectSameRotation(actual[index].rotation, expected[index].rotation, 1e-5f);
    for (int axis = 0; axis < 3; axis++) {
      expect(actual[index].translation[axis]).toBe(expected[index].translation[axis], 2e-2f);
    }
  }
}

static void rotateRestLocal(Skeleton *skeleton, size_t joint, float degrees, vec3 axis) {
  Transform rest = *Skeleton_RestLocal(skeleton, joint);
  versor turn;
  glm_quatv(turn, glm_rad(degrees), axis);
  glm_quat_mul(rest.rotation, turn, rest.rotation);
  Skeleton_SetRestLocal(skeleton, joint, &rest);
}

static void hipsDisplacement(size_t hips, vec3 dest) {
  glm_vec3_sub(Pose_Global(destination_pose, hips)->translation,
    (float *)Skeleton_RestGlobal(destination, hips)->translation, dest);
}

static float horizontalLength(vec3 displacement, vec3 up) {
  vec3 vertical, horizontal;
  glm_vec3_scale(up, glm_vec3_dot(displacement, up), vertical);
  glm_vec3_sub(displacement, vertical, horizontal);
  return glm_vec3_norm(horizontal);
}

describe("Retarget", []() {
  afterEach([]() {
    Retarget_Destroy(retarget);
    Pose_Destroy(source_pose);
    Pose_Destroy(destination_pose);
    AnimTrack_Destroy(track);
    BoneMap_Destroy(map);
    Skeleton_Destroy(source);
    Skeleton_Destroy(destination);
    GltfDoc_Destroy(source_doc);
    GltfDoc_Destroy(destination_doc);
    retarget = nullptr;
    source_pose = destination_pose = nullptr;
    track = nullptr;
    map = nullptr;
    source = destination = nullptr;
    source_doc = destination_doc = nullptr;
  });

  it("reproduces the source locals for identical skeletons", []() {
    loadSwordRun();
    createDefaultRetarget();
    for (size_t frame = 0; frame < 21; frame++) {
      runFrame(frame);
      for (size_t joint = 0; joint < Skeleton_JointCount(destination); joint++) {
        Transform *expected = Pose_Local(source_pose, joint);
        expectSameRotation(Pose_Local(destination_pose, joint)->rotation, expected->rotation, 1e-5f);
        for (int axis = 0; axis < 3; axis++) {
          expect(Pose_Local(destination_pose, joint)->translation[axis])
            .toBe(expected->translation[axis], 2e-2f);
        }
      }
    }
  });

  it("undoes a 90 degree, x0.01 change of source frame", []() {
    Transform frame_change;
    loadSwordRun();
    createDefaultRetarget();
    std::vector<Transform> plain = runTrack();
    Retarget_Destroy(retarget);
    Transform_Identity(&frame_change);
    glm_quatv(frame_change.rotation, glm_rad(90.0f), (vec3){1.0f, 0.0f, 0.0f});
    glm_vec3_fill(frame_change.scale, 0.01f);
    glm_vec3_copy((vec3){1.0f, 2.0f, 3.0f}, frame_change.translation);
    Skeleton_SetRootOffset(source, &frame_change);
    createDefaultRetarget();
    expect(Retarget_Alignment(retarget)->scale).toBe(100.0f, 1e-2f);
    expectSameLocals(runTrack(), plain);
  });

  it("brings a source A-pose arm back onto the destination T-pose", []() {
    loadSwordRun();
    size_t arm = Skeleton_FindJoint(source, "mixamorig:LeftArm");
    size_t fore_arm = Skeleton_FindJoint(source, "mixamorig:LeftForeArm");
    vec3 axis;
    glm_vec3_cross((float *)Skeleton_RestLocal(source, fore_arm)->translation, (vec3){0.0f, 0.0f, 1.0f}, axis);
    glm_vec3_normalize(axis);
    rotateRestLocal(source, arm, 45.0f, axis);
    createDefaultRetarget();
    for (size_t joint = 0; joint < Skeleton_JointCount(source); joint++) {
      *Pose_Local(source_pose, joint) = *Skeleton_RestLocal(destination, joint);
    }
    expect((int)Retarget_Frame(retarget, source_pose, destination_pose)).toBe((int)ERR_NONE);
    expectSameRotation(Pose_Local(destination_pose, arm)->rotation,
      Skeleton_RestLocal(destination, arm)->rotation, 1e-5f);
    expectSameRotation(Pose_Local(destination_pose, fore_arm)->rotation,
      Skeleton_RestLocal(destination, fore_arm)->rotation, 1e-3f);
  });

  it("adopts the source A-pose when the source is at its own rest", []() {
    loadSwordRun();
    size_t arm = Skeleton_FindJoint(source, "mixamorig:LeftArm");
    size_t fore_arm = Skeleton_FindJoint(source, "mixamorig:LeftForeArm");
    vec3 axis, source_direction, destination_direction;
    glm_vec3_cross((float *)Skeleton_RestLocal(source, fore_arm)->translation, (vec3){0.0f, 0.0f, 1.0f}, axis);
    glm_vec3_normalize(axis);
    rotateRestLocal(source, arm, 45.0f, axis);
    createDefaultRetarget();
    Pose_SetRest(source_pose, source);
    expect((int)Retarget_Frame(retarget, source_pose, destination_pose)).toBe((int)ERR_NONE);
    glm_vec3_sub((float *)Skeleton_RestGlobal(source, fore_arm)->translation,
      (float *)Skeleton_RestGlobal(source, arm)->translation, source_direction);
    glm_vec3_sub(Pose_Global(destination_pose, fore_arm)->translation,
      Pose_Global(destination_pose, arm)->translation, destination_direction);
    glm_vec3_normalize(source_direction);
    glm_vec3_normalize(destination_direction);
    expect(glm_vec3_dot(source_direction, destination_direction)).toBe(1.0f, 1e-3f);
  });

  it("leaves the A-pose uncorrected with rest alignment off", []() {
    RetargetOptions options;
    loadSwordRun();
    size_t arm = Skeleton_FindJoint(source, "mixamorig:LeftArm");
    size_t fore_arm = Skeleton_FindJoint(source, "mixamorig:LeftForeArm");
    vec3 axis;
    glm_vec3_cross((float *)Skeleton_RestLocal(source, fore_arm)->translation, (vec3){0.0f, 0.0f, 1.0f}, axis);
    glm_vec3_normalize(axis);
    rotateRestLocal(source, arm, 45.0f, axis);
    RetargetOptions_Default(&options);
    options.rest_align = 0;
    createRetarget(&options);
    for (size_t joint = 0; joint < Skeleton_JointCount(source); joint++) {
      *Pose_Local(source_pose, joint) = *Skeleton_RestLocal(destination, joint);
    }
    Retarget_Frame(retarget, source_pose, destination_pose);
    expect(fabsf(glm_quat_dot(Pose_Local(destination_pose, arm)->rotation,
      (float *)Skeleton_RestLocal(destination, arm)->rotation))).toBeLessThan(0.99f);
  });

  it("keeps unmapped destination joints at rest", []() {
    loadJog();
    createDefaultRetarget();
    runFrame(10);
    size_t head_top = Skeleton_FindJoint(destination, "mixamorig:HeadTop_End");
    expectSameRotation(Pose_Local(destination_pose, head_top)->rotation,
      Skeleton_RestLocal(destination, head_top)->rotation, 1e-6f);
  });

  it("folds root motion into the hips and strips it with in-place", []() {
    RetargetOptions options;
    vec3 displacement, up;
    loadJog();
    size_t hips = Skeleton_FindJoint(destination, "mixamorig:Hips");
    size_t last = AnimTrack_FrameCount(track, AnimTrack_Fps(track)) - 1;
    createDefaultRetarget();
    glm_quat_rotatev((float *)Retarget_Alignment(retarget)->rotation, (vec3){0.0f, 1.0f, 0.0f}, up);
    runFrame(last);
    hipsDisplacement(hips, displacement);
    expect(horizontalLength(displacement, up)).toBeGreaterThan(1000.0f);
    Retarget_Destroy(retarget);
    RetargetOptions_Default(&options);
    options.in_place = 1;
    createRetarget(&options);
    for (size_t frame = 0; frame <= last; frame++) {
      runFrame(frame);
      hipsDisplacement(hips, displacement);
      expect(horizontalLength(displacement, up)).toBeLessThan(1e-2f);
    }
  });

  it("emits sign-continuous rotations", []() {
    loadJog();
    createDefaultRetarget();
    std::vector<Transform> locals = runTrack();
    size_t joints = Skeleton_JointCount(destination);
    for (size_t index = joints; index < locals.size(); index++) {
      expect(glm_quat_dot(locals[index].rotation, locals[index - joints].rotation)).toBeGreaterThan(0.0f);
    }
  });

  it("applies frame overrides", []() {
    RetargetOptions options;
    versor expected;
    loadJog();
    RetargetOptions_Default(&options);
    options.has_frame_rotation = 1;
    glm_vec3_copy((vec3){-90.0f, 0.0f, 0.0f}, options.frame_rotation_degrees);
    options.has_frame_scale = 1;
    options.frame_scale = 450.0f;
    createRetarget(&options);
    glm_quatv(expected, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f});
    expectSameRotation(Retarget_Alignment(retarget)->rotation, expected, 1e-6f);
    expect(Retarget_Alignment(retarget)->scale).toBe(450.0f);
  });

  it("uses identity with frame alignment off", []() {
    RetargetOptions options;
    loadJog();
    RetargetOptions_Default(&options);
    options.frame_align = 0;
    createRetarget(&options);
    expect(Retarget_Alignment(retarget)->rotation[3]).toBe(1.0f);
    expect(Retarget_Alignment(retarget)->scale).toBe(1.0f);
  });

  it("reports a non-finite result as ERR_INTERNAL", []() {
    RetargetOptions options;
    loadSwordRun();
    RetargetOptions_Default(&options);
    options.has_frame_scale = 1;
    options.frame_scale = NAN;
    createRetarget(&options);
    AnimTrack_EvaluatePose(track, frameTime(3), source_pose);
    expect((int)Retarget_Frame(retarget, source_pose, destination_pose)).toBe((int)ERR_INTERNAL);
  });

  it("destroys NULL harmlessly", []() {
    Retarget_Destroy(NULL);
  });
});
