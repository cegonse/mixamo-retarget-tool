#include <cest>
#include <string>
extern "C" {
#include <skeleton.h>
}

static GltfDoc *doc = nullptr;
static Skeleton *skeleton = nullptr;

static void loadSkeleton(const char *path) {
  ErrorCode error = ERR_INTERNAL;
  doc = GltfDoc_Load(path, &error);
  expect(doc).toBeNotNull();
  skeleton = Skeleton_FromDoc(doc, 0, &error);
  expect((int)error).toBe((int)ERR_NONE);
  expect(skeleton).toBeNotNull();
}

static void expectGlobalMatchesCgltf(size_t joint) {
  mat4 expected, actual;
  GltfDoc_NodeRestWorldMatrix(doc, Skeleton_JointNode(skeleton, joint), expected);
  Transform_ToMat4(Skeleton_RestGlobal(skeleton, joint), actual);
  for (int column = 0; column < 4; column++) {
    for (int row = 0; row < 4; row++) {
      float tolerance = column == 3 ? 1e-2f : 1e-4f;
      expect(actual[column][row]).toBe(expected[column][row], tolerance);
    }
  }
}

describe("Skeleton", []() {
  afterEach([]() {
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
    skeleton = nullptr;
    doc = nullptr;
  });

  it("orders the 25 Mixamo joints depth-first, parents before children", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    expect(Skeleton_JointCount(skeleton)).toBe((size_t)25);
    expect(std::string(Skeleton_JointName(skeleton, 0))).toBe("mixamorig:Hips");
    expect(Skeleton_JointParent(skeleton, 0)).toBe(SKELETON_NO_JOINT);
    for (size_t joint = 1; joint < Skeleton_JointCount(skeleton); joint++) {
      expect(Skeleton_JointParent(skeleton, joint)).toBeLessThan(joint);
    }
    expect(std::string(Skeleton_JointName(skeleton, 1))).toBe("mixamorig:Spine");
    expect(Skeleton_JointNode(skeleton, 0)).toBe((size_t)24);
    expect(Skeleton_JointSkinIndex(skeleton, 0)).toBe((size_t)0);
  });

  it("finds joints by name and node", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    size_t left_hand = Skeleton_FindJoint(skeleton, "mixamorig:LeftHand");
    size_t left_fore_arm = Skeleton_FindJoint(skeleton, "mixamorig:LeftForeArm");
    expect(left_hand).Not->toBe(SKELETON_NO_JOINT);
    expect(Skeleton_JointParent(skeleton, left_hand)).toBe(left_fore_arm);
    expect(Skeleton_FindJoint(skeleton, "Hips")).toBe(SKELETON_NO_JOINT);
    expect(Skeleton_FindJointByNode(skeleton, 24)).toBe((size_t)0);
    expect(Skeleton_FindJointByNode(skeleton, 25)).toBe(SKELETON_NO_JOINT);
  });

  it("iterates children in node order", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    size_t spine2 = Skeleton_FindJoint(skeleton, "mixamorig:Spine2");
    expect(Skeleton_ChildCount(skeleton, 0)).toBe((size_t)3);
    expect(Skeleton_ChildCount(skeleton, spine2)).toBe((size_t)3);
    expect(Skeleton_Child(skeleton, 0, 0)).toBe((size_t)1);
    expect(Skeleton_Child(skeleton, 0, 3)).toBe(SKELETON_NO_JOINT);
    expect(Skeleton_ChildCount(skeleton, Skeleton_FindJoint(skeleton, "mixamorig:HeadTop_End")))
      .toBe((size_t)0);
  });

  it("computes hips global equal to local under the identity Armature", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    const Transform *local = Skeleton_RestLocal(skeleton, 0);
    const Transform *global = Skeleton_RestGlobal(skeleton, 0);
    for (int axis = 0; axis < 3; axis++) {
      expect(global->translation[axis]).toBe(local->translation[axis], 1e-5f);
    }
    expect(global->rotation[0]).toBe(local->rotation[0], 1e-6f);
  });

  it("composes spine global as hips times spine", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    Transform expected;
    Transform_Compose(Skeleton_RestLocal(skeleton, 0), Skeleton_RestLocal(skeleton, 1), &expected);
    for (int axis = 0; axis < 3; axis++) {
      expect(Skeleton_RestGlobal(skeleton, 1)->translation[axis]).toBe(expected.translation[axis], 1e-4f);
    }
  });

  it("matches cgltf world transforms for every Mixamo joint", []() {
    loadSkeleton(FIXTURES_DIR "/test_player.glb");
    for (size_t joint = 0; joint < Skeleton_JointCount(skeleton); joint++) {
      expectGlobalMatchesCgltf(joint);
    }
  });

  it("reads the 65-joint UAL rig with its rotated root", []() {
    loadSkeleton(FIXTURES_DIR "/UAL1_Standard_RM.glb");
    expect(Skeleton_JointCount(skeleton)).toBe((size_t)65);
    expect(std::string(Skeleton_JointName(skeleton, 0))).toBe("root");
    expect(std::string(Skeleton_JointName(skeleton, 1))).toBe("pelvis");
    expect(Skeleton_RestGlobal(skeleton, 1)->translation[1]).toBe(0.917f, 1e-3f);
    for (size_t joint = 0; joint < Skeleton_JointCount(skeleton); joint++) {
      expectGlobalMatchesCgltf(joint);
    }
  });

  it("reports a missing skin as ERR_NOT_FOUND", []() {
    ErrorCode error = ERR_NONE;
    doc = GltfDoc_Load(FIXTURES_DIR "/sword_run.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 3, &error);
    expect(skeleton).toBeNull();
    expect((int)error).toBe((int)ERR_NOT_FOUND);
  });

  it("destroys NULL harmlessly", []() {
    Skeleton_Destroy(NULL);
  });
});
