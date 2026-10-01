#include <cest>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
extern "C" {
#include <app_main.h>
#include <gltf_doc.h>
}

static const std::string library = FIXTURES_DIR "/UAL1_Standard_RM.glb";
static const std::string player = FIXTURES_DIR "/test_player.glb";
static const std::string ual_map = DOCS_DIR "/mappings/ual-to-mixamo.map";
static const std::string out_dir = std::string(TEST_OUTPUT_DIR) + "/library";
static FILE *captured = nullptr;

static int run(std::vector<std::string> arguments) {
  std::vector<char *> argv;
  arguments.insert(arguments.begin(), "anim-retarget");
  for (std::string &argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  return App_Run((int)arguments.size(), argv.data());
}

static std::string capturedText() {
  std::string text;
  char buffer[1024];
  rewind(captured);
  while (fgets(buffer, sizeof buffer, captured) != NULL) {
    text += buffer;
  }
  return text;
}

static GltfDoc *loadOutput(const std::string &path) {
  ErrorCode error = ERR_INTERNAL;
  GltfDoc *doc = GltfDoc_Load(path.c_str(), &error);
  expect((int)error).toBe((int)ERR_NONE);
  return doc;
}

static size_t sampleCount(GltfDoc *doc) {
  return GltfDoc_SamplerKeyCount(doc, 0, GltfDoc_Channel(doc, 0, 0).sampler);
}

describe("convert on the animation library", []() {
  beforeEach([]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
  });

  afterEach([]() {
    App_SetOutputStream(NULL);
    fclose(captured);
    std::filesystem::remove_all(out_dir);
  });

  it("writes one file per named track", []() {
    std::string dir = out_dir + "/named";
    expect(run({"convert", library, player, "--anim", "Idle_Loop,Walk_Loop", "--map-file", ual_map,
      "--out-dir", dir})).toBe(0);
    for (const char *name : {"Idle_Loop", "Walk_Loop"}) {
      GltfDoc *doc = loadOutput(dir + "/" + name + ".glb");
      expect(std::string(GltfDoc_AnimationName(doc, 0))).toBe(name);
      expect(GltfDoc_ChannelCount(doc, 0)).toBe((size_t)75);
      GltfDoc_Destroy(doc);
    }
    std::string text = capturedText();
    expect(text).toMatch("track \"Idle_Loop\": 76 frames @ 30 fps (2.500 s)\n");
    expect(text).toMatch("joints: 24 mapped, 41 source unmapped, 1 destination unmapped\n");
  });

  it("selects tracks by #index", []() {
    std::string out = out_dir + "/index/jog.glb";
    expect(run({"convert", library, player, "--anim", "#13", "--map-file", ual_map, "--out", out}))
      .toBe(0);
    GltfDoc *doc = loadOutput(out);
    expect(std::string(GltfDoc_AnimationName(doc, 0))).toBe("Jog_Fwd_Loop");
    GltfDoc_Destroy(doc);
  });

  it("resamples with --fps", []() {
    std::string out = out_dir + "/fps/jog.glb";
    expect(run({"convert", library, player, "--anim", "Jog_Fwd_Loop", "--map-file", ual_map,
      "--fps", "60", "--out", out})).toBe(0);
    GltfDoc *doc = loadOutput(out);
    expect(sampleCount(doc)).toBe((size_t)57);
    GltfDoc_Destroy(doc);
  });

  it("lists unmapped joints with --verbose", []() {
    expect(run({"convert", library, player, "--anim", "Hit_Head", "--map-file", ual_map,
      "--out-dir", out_dir + "/verbose", "--verbose"})).toBe(0);
    std::string text = capturedText();
    expect(text).toMatch("  source unmapped: root ");
    expect(text).toMatch("  destination unmapped: mixamorig:HeadTop_End\n");
  });

  it("rejects --out with more than one track", []() {
    expect(run({"convert", library, player, "--anim", "Idle_Loop,Walk_Loop", "--map-file", ual_map,
      "--out", out_dir + "/two.glb"})).toBe((int)ERR_BAD_ARGS);
  });

  it("rejects an unknown bone in the map", []() {
    expect(run({"convert", library, player, "--anim", "Idle_Loop", "--map-file", ual_map,
      "--map", "thumb_01_l=mixamorig:LeftThumb1", "--out-dir", out_dir})).toBe((int)ERR_NOT_FOUND);
  });

  it("converts all 43 tracks into files that load back", []() {
    std::string dir = out_dir + "/all";
    expect(run({"convert", library, player, "--all-anims", "--map-file", ual_map, "--out-dir", dir,
      "--in-place"})).toBe(0);
    ErrorCode error;
    GltfDoc *source = GltfDoc_Load(library.c_str(), &error);
    expect(GltfDoc_AnimationCount(source)).toBe((size_t)43);
    for (size_t animation = 0; animation < GltfDoc_AnimationCount(source); animation++) {
      std::string name = GltfDoc_AnimationName(source, animation);
      GltfDoc *doc = loadOutput(dir + "/" + name + ".glb");
      expect(std::string(GltfDoc_AnimationName(doc, 0))).toBe(name);
      expect(GltfDoc_SkinJointCount(doc, 0)).toBe((size_t)25);
      GltfDoc_Destroy(doc);
    }
    GltfDoc_Destroy(source);
  });
});
