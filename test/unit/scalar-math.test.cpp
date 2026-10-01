#include <cest>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
extern "C" {
#include <web_session.h>
}

static WebSession *session = nullptr;
static RetargetOptions options;

static std::string readText(const char *path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

static size_t animationNamed(const char *name) {
  for (size_t animation = 0; animation < WebSession_AnimationCount(session); animation++) {
    if (std::string(WebSession_AnimationName(session, animation)) == name) {
      return animation;
    }
  }
  return (size_t)-1;
}

static float degreesBetween(const float *a, const float *b) {
  float dot = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
  return 2.0f * acosf(fminf(1.0f, dot)) * 180.0f / (float)M_PI;
}

static float worstRestDeviation(AnimationClip *clip) {
  const Skeleton *destination = WebSession_Destination(session);
  float worst = 0.0f;
  for (size_t joint = 0; joint < AnimationClip_JointCount(clip); joint++) {
    const float *rotations = AnimationClip_Rotations(clip, joint);
    const float *rest = Skeleton_RestLocal(destination, joint)->rotation;
    for (size_t frame = 0; frame < AnimationClip_FrameCount(clip); frame++) {
      worst = fmaxf(worst, degreesBetween(&rotations[frame * 4], rest));
    }
  }
  return worst;
}

static void loadLibrary() {
  expect((int)WebSession_LoadSource(session, FIXTURES_DIR "/UAL1_Standard_RM.glb")).toBe((int)ERR_NONE);
  expect((int)WebSession_LoadDestination(session, FIXTURES_DIR "/test_player.glb")).toBe((int)ERR_NONE);
}

describe("retargeting on cglm's scalar code paths (no SSE, as in WebAssembly)", []() {
  beforeEach([]() {
    session = WebSession_Create();
    RetargetOptions_Default(&options);
    options.rest_align = 0;
  });

  afterEach([]() {
    WebSession_Destroy(session);
    session = nullptr;
  });

  it("is compiled without SSE", []() {
#if defined(__SSE__) || defined(__SSE2__)
    expect(true).toBe(false);
#endif
    expect(true).toBe(true);
  });

  it("maps A_TPose onto the destination rest pose", []() {
    ErrorCode error = ERR_INTERNAL;
    loadLibrary();
    std::string map = readText(DOCS_DIR "/mappings/ual-to-mixamo.map");
    expect((int)WebSession_Configure(session, map.c_str(), &options, 0.0f)).toBe((int)ERR_NONE);
    AnimationClip *clip = WebSession_Clip(session, animationNamed("A_TPose"), &error);
    float worst = clip != nullptr ? worstRestDeviation(clip) : 180.0f;
    AnimationClip_Destroy(clip);
    expect((int)error).toBe((int)ERR_NONE);
    expect(worst).toBeLessThan(1.0f);
  });

  it("applies a frame-rotation override the same way", []() {
    ErrorCode error = ERR_INTERNAL;
    loadLibrary();
    std::string map = readText(DOCS_DIR "/mappings/ual-to-mixamo.map");
    options.has_frame_rotation = 1;
    glm_vec3_copy((vec3){-91.0f, 0.1f, 0.0f}, options.frame_rotation_degrees);
    expect((int)WebSession_Configure(session, map.c_str(), &options, 0.0f)).toBe((int)ERR_NONE);
    AnimationClip *clip = WebSession_Clip(session, animationNamed("A_TPose"), &error);
    float worst = clip != nullptr ? worstRestDeviation(clip) : 180.0f;
    AnimationClip_Destroy(clip);
    expect((int)error).toBe((int)ERR_NONE);
    expect(worst).toBeLessThan(2.0f);
  });
});
