#include <cest>
#include <cmath>
#include <cstring>
#include <string>
extern "C" {
#include <file_io.h>
#include <glb_writer.h>
#include <json.h>
}

static GltfDoc *doc = nullptr;
static Skeleton *skeleton = nullptr;
static AnimationClip *clip = nullptr;
static ByteBuffer *bytes = nullptr;
static json_object *document = nullptr;
static const size_t frames = 5;

static uint32_t readUint32(const uint8_t *data) {
  return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void buildRestClip(float hips_x) {
  Pose *pose = Pose_Create(Skeleton_JointCount(skeleton));
  clip = AnimationClip_Create("Test/Track", Skeleton_JointCount(skeleton), frames, 30.0f);
  for (size_t frame = 0; frame < frames; frame++) {
    Pose_SetRest(pose, skeleton);
    Pose_Local(pose, 0)->translation[0] = hips_x;
    AnimationClip_SetFrame(clip, frame, pose);
  }
  Pose_Destroy(pose);
}

static void buildOutput(float hips_x) {
  ErrorCode error;
  doc = GltfDoc_Load(FIXTURES_DIR "/test_player.glb", &error);
  skeleton = Skeleton_FromDoc(doc, 0, &error);
  buildRestClip(hips_x);
  bytes = GlbWriter_Build(doc, skeleton, clip, &error);
  expect((int)error).toBe((int)ERR_NONE);
  uint32_t json_length = readUint32(ByteBuffer_Data(bytes) + 12);
  std::string json((const char *)ByteBuffer_Data(bytes) + 20, json_length);
  document = json_tokener_parse(json.c_str());
  expect(document).toBeNotNull();
}

static json_object *field(json_object *object, const char *key) {
  json_object *value = nullptr;
  json_object_object_get_ex(object, key, &value);
  return value;
}

static json_object *element(json_object *array, size_t index) {
  return json_object_array_get_idx(array, index);
}

static size_t length(json_object *array) {
  return json_object_array_length(array);
}

describe("GlbWriter", []() {
  afterEach([]() {
    json_object_put(document);
    ByteBuffer_Destroy(bytes);
    AnimationClip_Destroy(clip);
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
    document = nullptr;
    bytes = nullptr;
    clip = nullptr;
    skeleton = nullptr;
    doc = nullptr;
    FileIo_SetWriteBytesFunction(NULL);
  });

  it("copies the armature without the mesh node", []() {
    buildOutput(0.1f);
    json_object *nodes = field(document, "nodes");
    expect(length(nodes)).toBe((size_t)26);
    expect(std::string(json_object_get_string(field(element(nodes, 25), "name")))).toBe("Armature");
    expect(length(field(element(nodes, 25), "children"))).toBe((size_t)1);
    expect(json_object_get_int(element(field(element(nodes, 25), "children"), 0))).toBe(24);
    expect(json_object_get_int(element(field(element(field(document, "scenes"), 0), "nodes"), 0))).toBe(25);
    for (size_t node = 0; node < 26; node++) {
      expect(field(element(nodes, node), "mesh")).toBeNull();
      expect(field(element(nodes, node), "skin")).toBeNull();
    }
  });

  it("writes no model properties", []() {
    buildOutput(0.0f);
    expect(field(document, "meshes")).toBeNull();
    expect(field(document, "materials")).toBeNull();
    expect(field(document, "textures")).toBeNull();
    expect(field(document, "images")).toBeNull();
    expect(field(document, "extensionsUsed")).toBeNull();
    expect(std::string(json_object_get_string(field(field(document, "asset"), "generator"))))
      .toBe(GLB_WRITER_GENERATOR);
  });

  it("writes a T, R and S channel for every joint", []() {
    buildOutput(0.0f);
    json_object *animation = element(field(document, "animations"), 0);
    json_object *channels = field(animation, "channels"), *samplers = field(animation, "samplers");
    expect(std::string(json_object_get_string(field(animation, "name")))).toBe("Test/Track");
    expect(length(channels)).toBe((size_t)75);
    expect(std::string(json_object_get_string(field(field(element(channels, 2), "target"), "path")))).toBe("scale");
    expect(std::string(json_object_get_string(field(element(samplers, 0), "interpolation")))).toBe("LINEAR");
    expect(std::string(json_object_get_string(field(element(samplers, 1), "interpolation")))).toBe("LINEAR");
    expect(std::string(json_object_get_string(field(element(samplers, 2), "interpolation")))).toBe("STEP");
  });

  it("puts min and max on every animation input accessor", []() {
    buildOutput(0.0f);
    json_object *samplers = field(element(field(document, "animations"), 0), "samplers");
    json_object *accessors = field(document, "accessors");
    for (size_t sampler = 0; sampler < length(samplers); sampler++) {
      json_object *input = element(accessors, json_object_get_int(field(element(samplers, sampler), "input")));
      expect(field(input, "min")).toBeNotNull();
      expect(field(input, "max")).toBeNotNull();
    }
    json_object *times = element(accessors, 0);
    expect(json_object_get_double(element(field(times, "min"), 0))).toBe(0.0);
    expect(json_object_get_double(element(field(times, "max"), 0))).toBe(4.0 / 30.0, 1e-6);
  });

  it("aligns buffer views and sizes the buffer", []() {
    buildOutput(0.0f);
    json_object *views = field(document, "bufferViews");
    for (size_t view = 0; view < length(views); view++) {
      expect(json_object_get_int(field(element(views, view), "byteOffset")) % 4).toBe(0);
    }
    uint32_t json_length = readUint32(ByteBuffer_Data(bytes) + 12);
    uint32_t bin_length = readUint32(ByteBuffer_Data(bytes) + 20 + json_length);
    expect(json_length % 4).toBe((uint32_t)0);
    expect((int64_t)bin_length).toBe(json_object_get_int64(field(element(field(document, "buffers"), 0), "byteLength")));
  });

  it("copies the skin joints and inverse bind matrices", []() {
    buildOutput(0.0f);
    json_object *skin = element(field(document, "skins"), 0);
    json_object *ibm = element(field(document, "accessors"), json_object_get_int(field(skin, "inverseBindMatrices")));
    expect(length(field(skin, "joints"))).toBe((size_t)25);
    expect(json_object_get_int(element(field(skin, "joints"), 0))).toBe(24);
    expect(std::string(json_object_get_string(field(ibm, "type")))).toBe("MAT4");
    expect(json_object_get_int(field(ibm, "count"))).toBe(25);
  });

  it("parses back through cgltf with the same keys and IBMs", []() {
    buildOutput(0.1f);
    std::string path = std::string(TEST_OUTPUT_DIR) + "/glb-writer.glb";
    expect(FileIo_WriteBytes(path.c_str(), ByteBuffer_Data(bytes), ByteBuffer_Size(bytes))).toBe(0);
    ErrorCode error;
    GltfDoc *reloaded = GltfDoc_Load(path.c_str(), &error);
    expect((int)error).toBe((int)ERR_NONE);
    float original[16], copied[16];
    GltfDoc_SkinInverseBindMatrix(doc, 0, 7, original);
    GltfDoc_SkinInverseBindMatrix(reloaded, 0, 7, copied);
    expect(memcmp(original, copied, sizeof original)).toBe(0);
    GltfChannel hips = GltfDoc_Channel(reloaded, 0, 0);
    float values[15];
    GltfDoc_SamplerOutput(reloaded, 0, hips.sampler, values);
    expect(values[3]).toBe(0.1f, 0.0f);
    expect(GltfDoc_SamplerKeyCount(reloaded, 0, hips.sampler)).toBe(frames);
    GltfDoc_Destroy(reloaded);
    remove(path.c_str());
  });

  it("refuses to write a non-finite key", []() {
    ErrorCode error;
    doc = GltfDoc_Load(FIXTURES_DIR "/test_player.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
    buildRestClip(NAN);
    expect(GlbWriter_Build(doc, skeleton, clip, &error)).toBeNull();
    expect((int)error).toBe((int)ERR_INTERNAL);
  });

  it("reports a failed write as ERR_WRITE_OUTPUT", []() {
    ErrorCode error;
    size_t written = 0;
    doc = GltfDoc_Load(FIXTURES_DIR "/test_player.glb", &error);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
    buildRestClip(0.0f);
    FileIo_SetWriteBytesFunction([](const char *, const uint8_t *, size_t) { return -1; });
    expect((int)GlbWriter_Write("ignored.glb", doc, skeleton, clip, &written)).toBe((int)ERR_WRITE_OUTPUT);
  });
});
