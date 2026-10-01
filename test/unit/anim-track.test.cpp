#include <cest>
#include <string>
#include <vector>
extern "C" {
#include <anim_track.h>
}

static GltfDoc *doc = nullptr;
static Skeleton *skeleton = nullptr;
static AnimTrack *track = nullptr;

static void loadTrack(const char *path, size_t animation) {
  ErrorCode error = ERR_INTERNAL;
  doc = GltfDoc_Load(path, &error);
  skeleton = Skeleton_FromDoc(doc, 0, &error);
  track = AnimTrack_FromDoc(doc, animation, skeleton, &error);
  expect((int)error).toBe((int)ERR_NONE);
  expect(track).toBeNotNull();
}

static std::vector<float> rawOutput(size_t node, AnimationPath path) {
  for (size_t channel = 0; channel < GltfDoc_ChannelCount(doc, 0); channel++) {
    GltfChannel entry = GltfDoc_Channel(doc, 0, channel);
    if (entry.node == node && entry.path == path) {
      std::vector<float> values(GltfDoc_SamplerOutputFloatCount(doc, 0, entry.sampler));
      GltfDoc_SamplerOutput(doc, 0, entry.sampler, values.data());
      return values;
    }
  }
  return {};
}

describe("AnimTrack", []() {
  afterEach([]() {
    AnimTrack_Destroy(track);
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
    track = nullptr;
    skeleton = nullptr;
    doc = nullptr;
  });

  it("reads the sword_run timing: 21 frames at 30 fps", []() {
    loadTrack(FIXTURES_DIR "/sword_run.glb", 0);
    expect(std::string(AnimTrack_Name(track))).toBe("mixamo.com");
    expect(AnimTrack_Fps(track)).toBe(30.0f);
    expect(AnimTrack_Start(track)).toBe(1.0f / 30.0f, 1e-4f);
    expect(AnimTrack_End(track)).toBe(0.7f, 1e-5f);
    expect(AnimTrack_FrameCount(track, AnimTrack_Fps(track))).toBe((size_t)21);
    expect(AnimTrack_FrameCount(track, 60.0f)).toBe((size_t)41);
  });

  it("evaluates hips keys exactly at key times", []() {
    loadTrack(FIXTURES_DIR "/sword_run.glb", 0);
    std::vector<float> translation = rawOutput(24, ANIMATION_PATH_TRANSLATION);
    std::vector<float> rotation = rawOutput(24, ANIMATION_PATH_ROTATION);
    Transform local;
    AnimTrack_EvaluateLocal(track, 0, AnimTrack_Start(track) + 5.0f / 30.0f, &local);
    expect(local.translation[0]).toBe(translation[15], 1e-3f);
    expect(local.translation[2]).toBe(translation[17], 1e-3f);
    expect(fabsf(glm_quat_dot(local.rotation, &rotation[20]))).toBe(1.0f, 1e-5f);
  });

  it("interpolates halfway between hips translation keys", []() {
    loadTrack(FIXTURES_DIR "/sword_run.glb", 0);
    std::vector<float> translation = rawOutput(24, ANIMATION_PATH_TRANSLATION);
    Transform local;
    AnimTrack_EvaluateLocal(track, 0, AnimTrack_Start(track) + 1.5f / 30.0f, &local);
    expect(local.translation[1]).toBe(0.5f * (translation[4] + translation[7]), 1e-3f);
  });

  it("clamps outside the key range", []() {
    loadTrack(FIXTURES_DIR "/sword_run.glb", 0);
    std::vector<float> translation = rawOutput(24, ANIMATION_PATH_TRANSLATION);
    Transform local;
    AnimTrack_EvaluateLocal(track, 0, 0.0f, &local);
    expect(local.translation[0]).toBe(translation[0], 1e-5f);
    AnimTrack_EvaluateLocal(track, 0, 5.0f, &local);
    expect(local.translation[0]).toBe(translation[60], 1e-5f);
  });

  it("evaluates a whole pose and its globals", []() {
    loadTrack(FIXTURES_DIR "/sword_run.glb", 0);
    Pose *pose = Pose_Create(Skeleton_JointCount(skeleton));
    AnimTrack_EvaluatePose(track, 0.3f, pose);
    Pose_ComputeGlobals(pose, skeleton);
    expect(Pose_Global(pose, 0)->translation[2]).toBeInRange(-420.0f, -370.0f);
    expect(glm_quat_norm(Pose_Global(pose, 20)->rotation)).toBe(1.0f, 1e-5f);
    Pose_Destroy(pose);
  });

  it("reads a UAL track starting at zero", []() {
    loadTrack(FIXTURES_DIR "/UAL1_Standard_RM.glb", 13);
    expect(std::string(AnimTrack_Name(track))).toBe("Jog_Fwd_Loop");
    expect(AnimTrack_Start(track)).toBe(0.0f);
    expect(AnimTrack_Fps(track)).toBe(30.0f);
  });

  it("reports an out-of-range animation as ERR_NOT_FOUND", []() {
    ErrorCode error = ERR_NONE;
    doc = GltfDoc_Load(FIXTURES_DIR "/sword_run.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
    expect(AnimTrack_FromDoc(doc, 4, skeleton, &error)).toBeNull();
    expect((int)error).toBe((int)ERR_NOT_FOUND);
  });

  it("destroys NULL harmlessly", []() {
    AnimTrack_Destroy(NULL);
  });
});
