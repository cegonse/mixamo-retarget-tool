#include <cest>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
extern "C" {
#include <app_main.h>
#include <glb_container.h>
#include <gltf_doc.h>
#include <json.h>
}

static const std::string work_dir = std::string(TEST_OUTPUT_DIR) + "/frame-change";
static FILE *captured = nullptr;

static std::vector<uint8_t> readFile(const char *path) {
  std::vector<uint8_t> bytes;
  FILE *file = fopen(path, "rb");
  int character;
  while ((character = fgetc(file)) != EOF) {
    bytes.push_back((uint8_t)character);
  }
  fclose(file);
  return bytes;
}

static uint32_t readUint32(const uint8_t *data) {
  return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static json_object *floats(std::vector<double> values) {
  json_object *array = json_object_new_array();
  for (double value : values) {
    json_object_array_add(array, json_object_new_double(value));
  }
  return array;
}

static void writeTransformedSwordRun(const std::string &path) {
  std::vector<uint8_t> original = readFile(FIXTURES_DIR "/sword_run.glb");
  uint32_t json_length = readUint32(&original[12]);
  std::string json((const char *)&original[20], json_length);
  json_object *root = json_tokener_parse(json.c_str());
  json_object *nodes = nullptr;
  json_object_object_get_ex(root, "nodes", &nodes);
  json_object *armature = json_object_array_get_idx(nodes, 25);
  double half = std::sqrt(0.5);
  json_object_object_add(armature, "rotation", floats({half, 0.0, 0.0, half}));
  json_object_object_add(armature, "scale", floats({0.01, 0.01, 0.01}));
  json_object_object_add(armature, "translation", floats({1.0, -2.0, 3.0}));
  size_t new_length = 0;
  const char *text = json_object_to_json_string_length(root, JSON_C_TO_STRING_PLAIN, &new_length);
  ByteBuffer *bin = ByteBuffer_Create();
  uint32_t bin_length = readUint32(&original[20 + json_length]);
  ByteBuffer_AppendBytes(bin, &original[28 + json_length], bin_length);
  ErrorCode error;
  ByteBuffer *framed = GlbContainer_Frame(text, new_length, bin, &error);
  FILE *file = fopen(path.c_str(), "wb");
  fwrite(ByteBuffer_Data(framed), 1, ByteBuffer_Size(framed), file);
  fclose(file);
  ByteBuffer_Destroy(framed);
  ByteBuffer_Destroy(bin);
  json_object_put(root);
}

static int convert(const std::string &source, const std::string &out) {
  std::vector<std::string> arguments = {"anim-retarget", "convert", source,
    FIXTURES_DIR "/test_player.glb", "--anim", "mixamo.com", "--map-file",
    DOCS_DIR "/mappings/mixamo-identity.map", "--out", out};
  std::vector<char *> argv;
  for (std::string &argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  return App_Run((int)arguments.size(), argv.data());
}

static std::vector<float> allOutputs(GltfDoc *doc) {
  std::vector<float> values;
  for (size_t channel = 0; channel < GltfDoc_ChannelCount(doc, 0); channel++) {
    GltfChannel entry = GltfDoc_Channel(doc, 0, channel);
    std::vector<float> output(GltfDoc_SamplerOutputFloatCount(doc, 0, entry.sampler));
    GltfDoc_SamplerOutput(doc, 0, entry.sampler, output.data());
    values.insert(values.end(), output.begin(), output.end());
  }
  return values;
}

describe("convert with a changed source frame", []() {
  beforeEach([]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
  });

  afterEach([]() {
    App_SetOutputStream(NULL);
    fclose(captured);
  });

  it("matches the plain sword_run conversion", []() {
    std::string transformed = work_dir + "-source.glb";
    std::string plain_out = work_dir + "/plain.glb", changed_out = work_dir + "/changed.glb";
    writeTransformedSwordRun(transformed);
    expect(convert(FIXTURES_DIR "/sword_run.glb", plain_out)).toBe(0);
    expect(convert(transformed, changed_out)).toBe(0);
    ErrorCode error;
    GltfDoc *plain = GltfDoc_Load(plain_out.c_str(), &error);
    GltfDoc *changed = GltfDoc_Load(changed_out.c_str(), &error);
    std::vector<float> expected = allOutputs(plain), actual = allOutputs(changed);
    expect(actual.size()).toBe(expected.size());
    for (size_t index = 0; index < actual.size(); index++) {
      expect(actual[index]).toBe(expected[index], 2e-2f);
    }
    GltfDoc_Destroy(plain);
    GltfDoc_Destroy(changed);
    remove(transformed.c_str());
    std::filesystem::remove_all(work_dir);
  });
});
