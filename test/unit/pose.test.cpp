#include <cest>
extern "C" {
#include <pose.h>
}

static GltfDoc *doc = nullptr;
static Skeleton *skeleton = nullptr;
static Pose *pose = nullptr;

describe("Pose", []() {
  beforeEach([]() {
    ErrorCode error;
    doc = GltfDoc_Load(FIXTURES_DIR "/test_player.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
    pose = Pose_Create(Skeleton_JointCount(skeleton));
  });

  afterEach([]() {
    Pose_Destroy(pose);
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
    pose = nullptr;
    skeleton = nullptr;
    doc = nullptr;
  });

  it("reproduces the rest globals from the rest locals", []() {
    Pose_SetRest(pose, skeleton);
    Pose_ComputeGlobals(pose, skeleton);
    expect(Pose_JointCount(pose)).toBe((size_t)25);
    for (size_t joint = 0; joint < Pose_JointCount(pose); joint++) {
      const Transform *expected = Skeleton_RestGlobal(skeleton, joint);
      for (int axis = 0; axis < 3; axis++) {
        expect(Pose_Global(pose, joint)->translation[axis]).toBe(expected->translation[axis], 1e-4f);
      }
      expect(fabsf(glm_quat_dot(Pose_Global(pose, joint)->rotation, (float *)expected->rotation)))
        .toBe(1.0f, 1e-5f);
    }
  });

  it("propagates a hips rotation to descendants", []() {
    versor turn;
    vec3 offset, expected_spine;
    Pose_SetRest(pose, skeleton);
    glm_quatv(turn, glm_rad(90.0f), (vec3){0.0f, 0.0f, 1.0f});
    glm_quat_mul(turn, Pose_Local(pose, 0)->rotation, Pose_Local(pose, 0)->rotation);
    Pose_ComputeGlobals(pose, skeleton);
    glm_vec3_sub((float *)Skeleton_RestGlobal(skeleton, 1)->translation,
      (float *)Skeleton_RestGlobal(skeleton, 0)->translation, offset);
    glm_quat_rotatev(turn, offset, offset);
    glm_vec3_add((float *)Skeleton_RestGlobal(skeleton, 0)->translation, offset, expected_spine);
    for (int axis = 0; axis < 3; axis++) {
      expect(Pose_Global(pose, 1)->translation[axis]).toBe(expected_spine[axis], 1e-3f);
    }
  });

  it("destroys NULL harmlessly", []() {
    Pose_Destroy(NULL);
  });
});
