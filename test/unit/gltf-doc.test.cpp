#include <cest>
#include <cstdio>
#include <string>
#include <vector>
extern "C" {
#include <gltf_doc.h>
}

static const char *const player_path = FIXTURES_DIR "/test_player.glb";
static const char *const sword_path = FIXTURES_DIR "/sword_run.glb";
static GltfDoc *doc = nullptr;

static GltfDoc *loadOrFail(const char *path) {
  ErrorCode error = ERR_INTERNAL;
  GltfDoc *loaded = GltfDoc_Load(path, &error);
  expect((int)error).toBe((int)ERR_NONE);
  expect(loaded).toBeNotNull();
  return loaded;
}

static std::string writeTemporary(const char *name, const std::vector<unsigned char> &bytes) {
  std::string path = std::string(TEST_OUTPUT_DIR) + "/" + name;
  FILE *file = fopen(path.c_str(), "wb");
  fwrite(bytes.data(), 1, bytes.size(), file);
  fclose(file);
  return path;
}

static std::vector<unsigned char> readFixturePrefix(const char *path, size_t length) {
  std::vector<unsigned char> bytes(length);
  FILE *file = fopen(path, "rb");
  bytes.resize(fread(bytes.data(), 1, length, file));
  fclose(file);
  return bytes;
}

static size_t findChannel(GltfDoc *document, size_t node, AnimationPath path) {
  for (size_t channel = 0; channel < GltfDoc_ChannelCount(document, 0); channel++) {
    GltfChannel entry = GltfDoc_Channel(document, 0, channel);
    if (entry.node == node && entry.path == path) {
      return channel;
    }
  }
  return GLTF_NO_NODE;
}

describe("GltfDoc", []() {
  afterEach([]() {
    GltfDoc_Destroy(doc);
    doc = nullptr;
  });

  it("reads the asset of test_player.glb", []() {
    doc = loadOrFail(player_path);
    expect(std::string(GltfDoc_Version(doc))).toBe("2.0");
    expect(std::string(GltfDoc_Generator(doc))).toBe("Khronos glTF Blender I/O v5.1.19");
  });

  it("reads the node hierarchy of test_player.glb", []() {
    doc = loadOrFail(player_path);
    expect(GltfDoc_NodeCount(doc)).toBe((size_t)27);
    expect(std::string(GltfDoc_NodeName(doc, 26))).toBe("Armature");
    expect(std::string(GltfDoc_NodeName(doc, 24))).toBe("mixamorig:Hips");
    expect(GltfDoc_NodeParent(doc, 24)).toBe((size_t)26);
    expect(GltfDoc_NodeParent(doc, 26)).toBe(GLTF_NO_NODE);
    expect(GltfDoc_NodeChildCount(doc, 26)).toBe((size_t)2);
    expect(GltfDoc_NodeHasMesh(doc, 25)).toBeTruthy();
    expect(GltfDoc_NodeHasSkin(doc, 25)).toBeTruthy();
    expect(GltfDoc_NodeHasMesh(doc, 24)).toBeFalsy();
  });

  it("reads the hips rest transform", []() {
    Transform rest;
    doc = loadOrFail(player_path);
    expect((int)GltfDoc_NodeRest(doc, 24, &rest)).toBe((int)ERR_NONE);
    expect(rest.translation[0]).toBe(-5.53f, 1e-2f);
    expect(rest.translation[2]).toBe(-412.68f, 1e-2f);
    expect(rest.rotation[0]).toBe(-0.7071f, 1e-4f);
    expect(rest.rotation[3]).toBe(0.7071f, 1e-4f);
  });

  it("reads the skin of test_player.glb", []() {
    doc = loadOrFail(player_path);
    expect(GltfDoc_SkinCount(doc)).toBe((size_t)1);
    expect(std::string(GltfDoc_SkinName(doc, 0))).toBe("Armature");
    expect(GltfDoc_SkinJointCount(doc, 0)).toBe((size_t)25);
    expect(GltfDoc_SkinJoint(doc, 0, 0)).toBe((size_t)24);
    expect(std::string(GltfDoc_NodeName(doc, GltfDoc_SkinJoint(doc, 0, 1)))).toBe("mixamorig:Spine");
    expect(GltfDoc_SkinInverseBindAccessor(doc, 0)).toBe((size_t)48);
    expect(GltfDoc_ArmatureRoot(doc, 0)).toBe((size_t)26);
  });

  it("reads inverse bind matrices that undo the bind pose", []() {
    float inverse_bind[16];
    mat4 world, product;
    doc = loadOrFail(player_path);
    for (size_t joint = 0; joint < 25; joint++) {
      GltfDoc_SkinInverseBindMatrix(doc, 0, joint, inverse_bind);
      GltfDoc_NodeRestWorldMatrix(doc, GltfDoc_SkinJoint(doc, 0, joint), world);
      glm_mat4_mul(world, (vec4 *)inverse_bind, product);
      expect(product[0][0]).toBe(1.0f, 1e-3f);
      expect(product[3][2]).toBe(0.0f, 1e-2f);
    }
  });

  it("reads the animation of sword_run.glb", []() {
    doc = loadOrFail(sword_path);
    expect(GltfDoc_AnimationCount(doc)).toBe((size_t)1);
    expect(std::string(GltfDoc_AnimationName(doc, 0))).toBe("mixamo.com");
    expect(GltfDoc_ChannelCount(doc, 0)).toBe((size_t)75);
    expect(GltfDoc_ArmatureRoot(doc, 0)).toBe((size_t)25);
    expect(GltfDoc_SkinInverseBindAccessor(doc, 0)).toBe((size_t)76);
  });

  it("decodes translation keys and interpolation per channel kind", []() {
    doc = loadOrFail(sword_path);
    GltfChannel translation = GltfDoc_Channel(doc, 0, findChannel(doc, 24, ANIMATION_PATH_TRANSLATION));
    GltfChannel scale = GltfDoc_Channel(doc, 0, findChannel(doc, 24, ANIMATION_PATH_SCALE));
    std::vector<float> times(GltfDoc_SamplerKeyCount(doc, 0, translation.sampler));
    std::vector<float> values(GltfDoc_SamplerOutputFloatCount(doc, 0, translation.sampler));
    expect(times.size()).toBe((size_t)21);
    expect(values.size()).toBe((size_t)63);
    GltfDoc_SamplerInput(doc, 0, translation.sampler, times.data());
    GltfDoc_SamplerOutput(doc, 0, translation.sampler, values.data());
    expect(times.front()).toBe(0.0333f, 1e-3f);
    expect(times.back()).toBe(0.7f, 1e-4f);
    expect(values[2]).toBe(-388.5f, 1.0f);
    expect((int)GltfDoc_SamplerInterpolation(doc, 0, translation.sampler)).toBe((int)INTERPOLATION_LINEAR);
    expect((int)GltfDoc_SamplerInterpolation(doc, 0, scale.sampler)).toBe((int)INTERPOLATION_STEP);
    expect(GltfDoc_SamplerKeyCount(doc, 0, scale.sampler)).toBe((size_t)2);
  });

  it("decodes rotation keys as unit quaternions", []() {
    doc = loadOrFail(sword_path);
    GltfChannel rotation = GltfDoc_Channel(doc, 0, findChannel(doc, 13, ANIMATION_PATH_ROTATION));
    std::vector<float> values(GltfDoc_SamplerOutputFloatCount(doc, 0, rotation.sampler));
    expect(values.size()).toBe((size_t)84);
    GltfDoc_SamplerOutput(doc, 0, rotation.sampler, values.data());
    expect(glm_quat_norm(&values[4])).toBe(1.0f, 1e-4f);
  });

  it("reports a missing file as ERR_OPEN_INPUT", []() {
    ErrorCode error = ERR_NONE;
    doc = GltfDoc_Load(FIXTURES_DIR "/does_not_exist.glb", &error);
    expect(doc).toBeNull();
    expect((int)error).toBe((int)ERR_OPEN_INPUT);
  });

  it("reports garbage as ERR_BAD_GLB", []() {
    ErrorCode error = ERR_NONE;
    std::vector<unsigned char> garbage(64, 0xAB);
    std::string path = writeTemporary("garbage.glb", garbage);
    doc = GltfDoc_Load(path.c_str(), &error);
    expect(doc).toBeNull();
    expect((int)error).toBe((int)ERR_BAD_GLB);
    remove(path.c_str());
  });

  it("reports a truncated GLB as ERR_BAD_GLB", []() {
    ErrorCode error = ERR_NONE;
    std::string path = writeTemporary("truncated.glb", readFixturePrefix(sword_path, 4000));
    doc = GltfDoc_Load(path.c_str(), &error);
    expect(doc).toBeNull();
    expect((int)error).toBe((int)ERR_BAD_GLB);
    remove(path.c_str());
  });

  it("destroys NULL harmlessly", []() {
    GltfDoc_Destroy(NULL);
  });
});
