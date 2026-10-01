#include <cest>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
extern "C" {
#include <app_main.h>
#include <gltf_doc.h>
}

static const std::string output_dir = std::string(TEST_OUTPUT_DIR) + "/convert";
static FILE *captured = nullptr;
static GltfDoc *produced = nullptr;
static GltfDoc *reference = nullptr;

static int run(std::vector<std::string> arguments) {
  std::vector<char *> argv;
  arguments.insert(arguments.begin(), "anim-retarget");
  for (std::string &argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  return App_Run((int)arguments.size(), argv.data());
}

static GltfDoc *load(const std::string &path) {
  ErrorCode error = ERR_INTERNAL;
  GltfDoc *doc = GltfDoc_Load(path.c_str(), &error);
  expect((int)error).toBe((int)ERR_NONE);
  return doc;
}

static size_t nodeNamed(GltfDoc *doc, const std::string &name) {
  for (size_t node = 0; node < GltfDoc_NodeCount(doc); node++) {
    if (name == GltfDoc_NodeName(doc, node)) {
      return node;
    }
  }
  return GLTF_NO_NODE;
}

static std::vector<float> keys(GltfDoc *doc, const std::string &name, AnimationPath path,
    bool times) {
  size_t node = nodeNamed(doc, name);
  for (size_t channel = 0; channel < GltfDoc_ChannelCount(doc, 0); channel++) {
    GltfChannel entry = GltfDoc_Channel(doc, 0, channel);
    if (entry.node == node && entry.path == path) {
      size_t count = times ? GltfDoc_SamplerKeyCount(doc, 0, entry.sampler)
        : GltfDoc_SamplerOutputFloatCount(doc, 0, entry.sampler);
      std::vector<float> values(count);
      if (times) {
        GltfDoc_SamplerInput(doc, 0, entry.sampler, values.data());
      } else {
        GltfDoc_SamplerOutput(doc, 0, entry.sampler, values.data());
      }
      return values;
    }
  }
  return {};
}

static float angleDegrees(const float *a, const float *b) {
  float dot = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
  return 2.0f * acosf(fminf(1.0f, dot)) * 180.0f / (float)M_PI;
}

static void expectSameIbms(GltfDoc *a, GltfDoc *b) {
  float left[16], right[16];
  expect(GltfDoc_SkinJointCount(a, 0)).toBe(GltfDoc_SkinJointCount(b, 0));
  for (size_t joint = 0; joint < GltfDoc_SkinJointCount(a, 0); joint++) {
    GltfDoc_SkinInverseBindMatrix(a, 0, joint, left);
    GltfDoc_SkinInverseBindMatrix(b, 0, joint, right);
    expect(memcmp(left, right, sizeof left)).toBe(0);
    expect(std::string(GltfDoc_NodeName(a, GltfDoc_SkinJoint(a, 0, joint))))
      .toBe(GltfDoc_NodeName(b, GltfDoc_SkinJoint(b, 0, joint)));
  }
}

describe("convert command", []() {
  beforeEach([]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
  });

  afterEach([]() {
    App_SetOutputStream(NULL);
    fclose(captured);
    GltfDoc_Destroy(produced);
    GltfDoc_Destroy(reference);
    produced = reference = nullptr;
    remove((output_dir + "/identity.glb").c_str());
    remove((output_dir + "/sword_run.glb").c_str());
    remove((output_dir + "/mixamo.com.glb").c_str());
  });

  it("maps test_player onto itself unchanged", []() {
    std::string out = output_dir + "/identity.glb";
    expect(run({"convert", FIXTURES_DIR "/test_player.glb", FIXTURES_DIR "/test_player.glb",
      "--anim", "mixamo.com", "--map-file", DOCS_DIR "/mappings/mixamo-identity.map", "--out", out}))
      .toBe(0);
    produced = load(out);
    reference = load(FIXTURES_DIR "/test_player.glb");
    expectSameIbms(produced, reference);
    for (size_t joint = 0; joint < 25; joint++) {
      std::string name = GltfDoc_NodeName(reference, GltfDoc_SkinJoint(reference, 0, joint));
      std::vector<float> expected = keys(reference, name, ANIMATION_PATH_ROTATION, false);
      std::vector<float> actual = keys(produced, name, ANIMATION_PATH_ROTATION, false);
      expect(actual.size()).toBe(expected.size());
      for (size_t key = 0; key + 3 < actual.size(); key += 4) {
        float dot = actual[key] * expected[key] + actual[key + 1] * expected[key + 1]
          + actual[key + 2] * expected[key + 2] + actual[key + 3] * expected[key + 3];
        expect(fabsf(dot)).toBe(1.0f, 1e-5f);
      }
    }
  });

  it("retargets sword_run onto test_player within the rest-pose difference", []() {
    std::string out = output_dir + "/sword_run.glb";
    expect(run({"convert", FIXTURES_DIR "/sword_run.glb", FIXTURES_DIR "/test_player.glb",
      "--anim", "mixamo.com", "--map-file", DOCS_DIR "/mappings/mixamo-identity.map", "--out", out}))
      .toBe(0);
    produced = load(out);
    reference = load(FIXTURES_DIR "/sword_run.glb");
    std::vector<float> times = keys(produced, "mixamorig:Hips", ANIMATION_PATH_TRANSLATION, true);
    expect(times.size()).toBe((size_t)21);
    expect(times.front()).toBe(0.0f);
    expect(times.back()).toBe(20.0f / 30.0f, 1e-6f);
    expect(keys(produced, "mixamorig:LeftHand", ANIMATION_PATH_SCALE, false).size()).toBe((size_t)6);
    size_t compared = 0;
    for (size_t joint = 0; joint < 25; joint++) {
      std::string name = GltfDoc_NodeName(reference, GltfDoc_SkinJoint(reference, 0, joint));
      std::vector<float> expected = keys(reference, name, ANIMATION_PATH_ROTATION, false);
      std::vector<float> actual = keys(produced, name, ANIMATION_PATH_ROTATION, false);
      if (expected.size() != actual.size()) {
        continue;
      }
      compared++;
      for (size_t key = 0; key < actual.size(); key += 4) {
        expect(angleDegrees(&actual[key], &expected[key])).toBeLessThan(3.0f);
      }
    }
    expect(compared).toBeGreaterThan((size_t)15);
  });

  it("keeps the sword_run hips path within one unit of its rest offset", []() {
    std::string out = output_dir + "/sword_run.glb";
    run({"convert", FIXTURES_DIR "/sword_run.glb", FIXTURES_DIR "/test_player.glb", "--anim",
      "mixamo.com", "--map-file", DOCS_DIR "/mappings/mixamo-identity.map", "--out", out});
    produced = load(out);
    reference = load(FIXTURES_DIR "/sword_run.glb");
    Transform produced_rest, reference_rest;
    GltfDoc_NodeRest(produced, nodeNamed(produced, "mixamorig:Hips"), &produced_rest);
    GltfDoc_NodeRest(reference, nodeNamed(reference, "mixamorig:Hips"), &reference_rest);
    std::vector<float> expected = keys(reference, "mixamorig:Hips", ANIMATION_PATH_TRANSLATION, false);
    std::vector<float> actual = keys(produced, "mixamorig:Hips", ANIMATION_PATH_TRANSLATION, false);
    for (size_t key = 0; key < actual.size(); key += 3) {
      for (int axis = 0; axis < 3; axis++) {
        expect(actual[key + axis] - produced_rest.translation[axis])
          .toBe(expected[key + axis] - reference_rest.translation[axis], 1.0f);
      }
    }
  });

  it("names the output after the track in --out-dir", []() {
    expect(run({"convert", FIXTURES_DIR "/sword_run.glb", FIXTURES_DIR "/test_player.glb",
      "--anim", "mixamo.com", "--map-file", DOCS_DIR "/mappings/mixamo-identity.map", "--out-dir",
      output_dir})).toBe(0);
    produced = load(output_dir + "/mixamo.com.glb");
    expect(std::string(GltfDoc_AnimationName(produced, 0))).toBe("mixamo.com");
  });

  it("fails cleanly on a garbage source", []() {
    std::string garbage = output_dir + "-garbage.glb";
    FILE *file = fopen(garbage.c_str(), "wb");
    fputs("glTF this is not a real binary file at all", file);
    fclose(file);
    expect(run({"convert", garbage, FIXTURES_DIR "/test_player.glb", "--anim", "x", "--map",
      "a=b", "--out-dir", output_dir})).toBe(3);
    remove(garbage.c_str());
  });

  it("fails cleanly on an unknown track", []() {
    expect(run({"convert", FIXTURES_DIR "/sword_run.glb", FIXTURES_DIR "/test_player.glb",
      "--anim", "Nope", "--map-file", DOCS_DIR "/mappings/mixamo-identity.map", "--out-dir",
      output_dir})).toBe(4);
  });
});
