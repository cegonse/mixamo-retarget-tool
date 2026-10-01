#include <cest>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
extern "C" {
#include <file_io.h>
#include <gltf_doc.h>
#include <web_session.h>
}

static const char *player = FIXTURES_DIR "/test_player.glb";
static const char *sword_run = FIXTURES_DIR "/sword_run.glb";
static const char *library = FIXTURES_DIR "/UAL1_Standard_RM.glb";
static const std::string output_path = std::string(TEST_OUTPUT_DIR) + "/web-session.glb";
static WebSession *session = nullptr;
static RetargetOptions options;

static std::string readText(const char *path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

static std::string identityMap() {
  return readText(DOCS_DIR "/mappings/mixamo-identity.map");
}

static std::string libraryMap() {
  return readText(DOCS_DIR "/mappings/ual-to-mixamo.map");
}

static void loadBoth(const char *source) {
  expect((int)WebSession_LoadSource(session, source)).toBe((int)ERR_NONE);
  expect((int)WebSession_LoadDestination(session, player)).toBe((int)ERR_NONE);
}

static size_t animationNamed(const char *name) {
  for (size_t animation = 0; animation < WebSession_AnimationCount(session); animation++) {
    if (std::string(WebSession_AnimationName(session, animation)) == name) {
      return animation;
    }
  }
  return (size_t)-1;
}

describe("WebSession", []() {
  beforeEach([]() {
    session = WebSession_Create();
    RetargetOptions_Default(&options);
  });

  afterEach([]() {
    WebSession_Destroy(session);
    session = nullptr;
    remove(output_path.c_str());
  });

  it("starts empty and not ready", []() {
    expect(WebSession_AnimationCount(session)).toBe((size_t)0);
    expect(WebSession_IsReady(session)).toBe(0);
    expect(WebSession_Alignment(session)).toBeNull();
    expect(WebSession_Destination(session)).toBeNull();
  });

  it("lists the source animations before a destination exists", []() {
    TrackTiming timing;
    expect((int)WebSession_LoadSource(session, sword_run)).toBe((int)ERR_NONE);
    expect(WebSession_AnimationCount(session)).toBe((size_t)1);
    expect(std::string(WebSession_AnimationName(session, 0))).toBe("mixamo.com");
    expect((int)WebSession_AnimationTiming(session, 0, &timing)).toBe((int)ERR_NONE);
    expect(timing.end - timing.start).toBe(0.6667f, 1e-3f);
    expect(timing.fps).toBe(30.0f, 1e-4f);
    expect(WebSession_IsReady(session)).toBe(0);
  });

  it("reports a missing file", []() {
    expect((int)WebSession_LoadSource(session, FIXTURES_DIR "/missing.glb")).toBe((int)ERR_OPEN_INPUT);
    expect(WebSession_AnimationCount(session)).toBe((size_t)0);
  });

  it("becomes ready once both rigs and a map are configured", []() {
    loadBoth(sword_run);
    expect(WebSession_IsReady(session)).toBe(0);
    expect((int)WebSession_Configure(session, identityMap().c_str(), &options, 0.0f)).toBe((int)ERR_NONE);
    expect(WebSession_IsReady(session)).toBe(1);
    expect(WebSession_Alignment(session)->scale).toBe(1.0f, 0.05f);
    expect(Skeleton_JointCount(WebSession_Destination(session))).toBe((size_t)25);
  });

  it("stays ready when a rig is loaded again", []() {
    loadBoth(sword_run);
    WebSession_Configure(session, identityMap().c_str(), &options, 0.0f);
    expect((int)WebSession_LoadSource(session, sword_run)).toBe((int)ERR_NONE);
    expect(WebSession_IsReady(session)).toBe(1);
    expect((int)WebSession_LoadDestination(session, player)).toBe((int)ERR_NONE);
    expect(WebSession_IsReady(session)).toBe(1);
  });

  it("is not ready after a bad map and recovers on a good one", []() {
    loadBoth(sword_run);
    expect((int)WebSession_Configure(session, "nobody=mixamorig:Hips", &options, 0.0f))
      .toBe((int)ERR_NOT_FOUND);
    expect(WebSession_IsReady(session)).toBe(0);
    expect((int)WebSession_Configure(session, identityMap().c_str(), &options, 0.0f)).toBe((int)ERR_NONE);
    expect(WebSession_IsReady(session)).toBe(1);
  });

  it("refuses a clip before it is ready", []() {
    ErrorCode error = ERR_NONE;
    loadBoth(sword_run);
    expect(WebSession_Clip(session, 0, &error)).toBeNull();
    expect((int)error).toBe((int)ERR_NOT_FOUND);
  });

  it("builds a retargeted clip on the inferred grid", []() {
    ErrorCode error = ERR_INTERNAL;
    loadBoth(sword_run);
    WebSession_Configure(session, identityMap().c_str(), &options, 0.0f);
    AnimationClip *clip = WebSession_Clip(session, 0, &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)21);
    expect(AnimationClip_JointCount(clip)).toBe((size_t)25);
    AnimationClip_Destroy(clip);
  });

  it("honours the fps override", []() {
    ErrorCode error = ERR_INTERNAL;
    loadBoth(sword_run);
    WebSession_Configure(session, identityMap().c_str(), &options, 60.0f);
    AnimationClip *clip = WebSession_Clip(session, 0, &error);
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)41);
    AnimationClip_Destroy(clip);
  });

  it("builds a GLB that loads back with the track", []() {
    ErrorCode error = ERR_INTERNAL;
    loadBoth(sword_run);
    WebSession_Configure(session, identityMap().c_str(), &options, 0.0f);
    ByteBuffer *bytes = WebSession_Glb(session, 0, &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(FileIo_WriteBytes(output_path.c_str(), ByteBuffer_Data(bytes), ByteBuffer_Size(bytes))).toBe(0);
    ByteBuffer_Destroy(bytes);
    GltfDoc *doc = GltfDoc_Load(output_path.c_str(), &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(GltfDoc_AnimationCount(doc)).toBe((size_t)1);
    expect(std::string(GltfDoc_AnimationName(doc, 0))).toBe("mixamo.com");
    expect(GltfDoc_SkinJointCount(doc, 0)).toBe((size_t)25);
    expect(GltfDoc_ChannelCount(doc, 0)).toBe((size_t)75);
    GltfDoc_Destroy(doc);
  });

  it("names output files like the CLI", []() {
    WebSession_LoadSource(session, sword_run);
    char *name = WebSession_AnimationFileName(session, 0);
    expect(std::string(name)).toBe("mixamo.com.glb");
    free(name);
  });

  it("converts a track of the real library in place", []() {
    ErrorCode error = ERR_INTERNAL;
    loadBoth(library);
    options.in_place = 1;
    options.rest_align = 0;
    expect((int)WebSession_Configure(session, libraryMap().c_str(), &options, 0.0f)).toBe((int)ERR_NONE);
    expect(WebSession_AnimationCount(session)).toBe((size_t)43);
    expect(WebSession_Alignment(session)->scale).toBeInRange(380.0f, 420.0f);
    size_t jog = animationNamed("Jog_Fwd_Loop");
    expect(jog).Not->toBe((size_t)-1);
    AnimationClip *clip = WebSession_Clip(session, jog, &error);
    expect((int)error).toBe((int)ERR_NONE);
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)29);
    const float *hips = AnimationClip_Translations(clip, 0);
    size_t last = AnimationClip_FrameCount(clip) - 1;
    expect(fabsf(hips[last * 3 + 1] - hips[1])).toBeLessThan(5.0f);
    AnimationClip_Destroy(clip);
  });

  it("destroys NULL harmlessly", []() {
    WebSession_Destroy(NULL);
  });
});
