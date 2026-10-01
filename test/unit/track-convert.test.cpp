#include <cest>
#include <cstring>
#include <string>
extern "C" {
#include <track_convert.h>
}

static ConvertSession session;
static Args args;
static bool opened = false;

static void openSession(const char *source) {
  memset(&args, 0, sizeof args);
  args.command = COMMAND_CONVERT;
  args.source_path = source;
  args.destination_path = FIXTURES_DIR "/test_player.glb";
  args.map_sources[0].is_file = 1;
  args.map_sources[0].text = DOCS_DIR "/mappings/mixamo-identity.map";
  args.map_source_count = 1;
  RetargetOptions_Default(&args.retarget);
  opened = ConvertSession_Open(&session, &args) == ERR_NONE;
}

describe("TrackConvert", []() {
  beforeEach([]() {
    openSession(FIXTURES_DIR "/sword_run.glb");
  });

  afterEach([]() {
    if (opened) {
      ConvertSession_Close(&session);
    }
    opened = false;
  });

  it("samples a track on the inferred grid", []() {
    ErrorCode error = ERR_INTERNAL;
    AnimationClip *clip = TrackConvert_Clip(&session, 0, 0, 0.0f, &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(clip).toBeNotNull();
    expect(std::string(AnimationClip_Name(clip))).toBe("mixamo.com");
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)21);
    expect(AnimationClip_JointCount(clip)).toBe((size_t)25);
    expect(AnimationClip_Fps(clip)).toBe(30.0f, 1e-4f);
    AnimationClip_Destroy(clip);
  });

  it("honours an fps override", []() {
    ErrorCode error = ERR_INTERNAL;
    AnimationClip *clip = TrackConvert_Clip(&session, 0, 1, 60.0f, &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)41);
    expect(AnimationClip_Fps(clip)).toBe(60.0f, 1e-4f);
    AnimationClip_Destroy(clip);
  });

  it("keeps the hips at the destination rest on the identity map", []() {
    ErrorCode error = ERR_INTERNAL;
    AnimationClip *clip = TrackConvert_Clip(&session, 0, 0, 0.0f, &error);
    const float *hips = AnimationClip_Translations(clip, 0);
    const Transform *rest = Skeleton_RestLocal(session.destination, 0);
    float distance = 0.0f;
    for (int axis = 0; axis < 3; axis++) {
      distance += (hips[axis] - rest->translation[axis]) * (hips[axis] - rest->translation[axis]);
    }
    expect(distance).toBeLessThan(60.0f * 60.0f);
    AnimationClip_Destroy(clip);
  });
});
