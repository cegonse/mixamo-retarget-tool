#include <cest>
#include <cmath>
extern "C" {
#include <web_pose.h>
}

static GltfDoc *doc = nullptr;
static Skeleton *skeleton = nullptr;
static AnimationClip *clip = nullptr;
static WebPoseSet *poses = nullptr;
static const size_t frames = 3;

static void buildClip(float hips_shift_per_frame) {
  Pose *pose = Pose_Create(Skeleton_JointCount(skeleton));
  clip = AnimationClip_Create("rest", Skeleton_JointCount(skeleton), frames, 30.0f);
  for (size_t frame = 0; frame < frames; frame++) {
    Pose_SetRest(pose, skeleton);
    Pose_Local(pose, 0)->translation[0] += hips_shift_per_frame * (float)frame;
    AnimationClip_SetFrame(clip, frame, pose);
  }
  Pose_Destroy(pose);
}

describe("WebPoseSet", []() {
  beforeEach([]() {
    ErrorCode error;
    doc = GltfDoc_Load(FIXTURES_DIR "/test_player.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
  });

  afterEach([]() {
    WebPoseSet_Destroy(poses);
    AnimationClip_Destroy(clip);
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
    poses = nullptr;
    clip = nullptr;
    skeleton = nullptr;
    doc = nullptr;
  });

  it("stores world-space rest transforms in skin order", []() {
    buildClip(0.0f);
    poses = WebPoseSet_FromClip(clip, skeleton);
    expect(poses).toBeNotNull();
    expect(WebPoseSet_FrameCount(poses)).toBe(frames);
    expect(WebPoseSet_JointCount(poses)).toBe((size_t)25);
    expect(WebPoseSet_Fps(poses)).toBe(30.0f, 1e-4f);
    for (size_t joint = 0; joint < Skeleton_JointCount(skeleton); joint++) {
      const Transform *rest = Skeleton_RestGlobal(skeleton, joint);
      const float *stored = WebPoseSet_Joint(poses, frames - 1, Skeleton_JointSkinIndex(skeleton, joint));
      for (int axis = 0; axis < 3; axis++) {
        expect(stored[axis]).toBe(rest->translation[axis], 1e-3f);
        expect(stored[7 + axis]).toBe(rest->scale[axis], 1e-4f);
      }
      expect(fabsf(stored[3] * rest->rotation[0] + stored[4] * rest->rotation[1]
        + stored[5] * rest->rotation[2] + stored[6] * rest->rotation[3])).toBe(1.0f, 1e-4f);
    }
  });

  it("moves every joint with the hips", []() {
    buildClip(10.0f);
    poses = WebPoseSet_FromClip(clip, skeleton);
    for (size_t skin_index = 0; skin_index < 25; skin_index++) {
      expect(WebPoseSet_Joint(poses, 2, skin_index)[0] - WebPoseSet_Joint(poses, 0, skin_index)[0])
        .toBe(20.0f, 1e-3f);
    }
  });

  it("refuses a clip whose joint count differs from the skeleton", []() {
    clip = AnimationClip_Create("odd", 3, frames, 30.0f);
    expect(WebPoseSet_FromClip(clip, skeleton)).toBeNull();
  });

  it("destroys NULL harmlessly", []() {
    WebPoseSet_Destroy(NULL);
  });
});
