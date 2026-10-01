#include <cest>
extern "C" {
#include <frame_align.h>
#include <rest_align.h>
}

static GltfDoc *source_doc = nullptr, *destination_doc = nullptr;
static Skeleton *source = nullptr, *destination = nullptr;
static BoneMap *map = nullptr;

static void loadRigs(const char *source_path, const char *destination_path, const char *map_path) {
  ErrorCode error;
  source_doc = GltfDoc_Load(source_path, &error);
  destination_doc = GltfDoc_Load(destination_path, &error);
  source = Skeleton_FromDoc(source_doc, 0, &error);
  destination = Skeleton_FromDoc(destination_doc, 0, &error);
  map = BoneMap_Create();
  BoneMap_AddFile(map, map_path);
  expect((int)BoneMap_Resolve(map, source, destination)).toBe((int)ERR_NONE);
}

static size_t pairFor(const char *source_name) {
  return BoneMap_PairForSource(map, Skeleton_FindJoint(source, source_name));
}

describe("RestAlign", []() {
  afterEach([]() {
    BoneMap_Destroy(map);
    Skeleton_Destroy(source);
    Skeleton_Destroy(destination);
    GltfDoc_Destroy(source_doc);
    GltfDoc_Destroy(destination_doc);
    map = nullptr;
    source = destination = nullptr;
    source_doc = destination_doc = nullptr;
  });

  it("is identity for a rig mapped onto itself", []() {
    versor corrections[25];
    versor identity = GLM_QUAT_IDENTITY_INIT;
    loadRigs(FIXTURES_DIR "/test_player.glb", FIXTURES_DIR "/test_player.glb",
      DOCS_DIR "/mappings/mixamo-identity.map");
    RestAlign_Compute(map, source, destination, identity, corrections);
    for (size_t pair = 0; pair < 25; pair++) {
      expect(fabsf(glm_quat_dot(corrections[pair], identity))).toBe(1.0f, 1e-5f);
    }
  });

  it("brings each single-child source bone onto the destination direction", []() {
    versor corrections[24];
    FrameAlignment alignment;
    loadRigs(FIXTURES_DIR "/UAL1_Standard_RM.glb", FIXTURES_DIR "/test_player.glb",
      DOCS_DIR "/mappings/ual-to-mixamo.map");
    FrameAlign_FromMap(map, source, destination, &alignment);
    RestAlign_Compute(map, source, destination, alignment.rotation, corrections);
    for (size_t pair = 0; pair < BoneMap_PairCount(map); pair++) {
      if (BoneMap_MappedChildCount(map, pair) != 1) {
        continue;
      }
      size_t child = BoneMap_MappedChild(map, pair, 0);
      vec3 source_direction, destination_direction;
      glm_vec3_sub((float *)Skeleton_RestGlobal(source, BoneMap_SourceJoint(map, child))->translation,
        (float *)Skeleton_RestGlobal(source, BoneMap_SourceJoint(map, pair))->translation, source_direction);
      glm_vec3_sub((float *)Skeleton_RestGlobal(destination, BoneMap_DestinationJoint(map, child))->translation,
        (float *)Skeleton_RestGlobal(destination, BoneMap_DestinationJoint(map, pair))->translation,
        destination_direction);
      glm_quat_rotatev(alignment.rotation, source_direction, source_direction);
      glm_quat_rotatev(corrections[pair], source_direction, source_direction);
      glm_vec3_normalize(source_direction);
      glm_vec3_normalize(destination_direction);
      expect(glm_vec3_dot(source_direction, destination_direction)).toBe(1.0f, 1e-4f);
    }
  });

  it("lets leaves and forks inherit from their mapped parent", []() {
    versor corrections[24];
    FrameAlignment alignment;
    loadRigs(FIXTURES_DIR "/UAL1_Standard_RM.glb", FIXTURES_DIR "/test_player.glb",
      DOCS_DIR "/mappings/ual-to-mixamo.map");
    FrameAlign_FromMap(map, source, destination, &alignment);
    RestAlign_Compute(map, source, destination, alignment.rotation, corrections);
    size_t hand = pairFor("hand_l"), fore_arm = pairFor("lowerarm_l");
    size_t spine = pairFor("spine_03"), parent = pairFor("spine_02"), pelvis = pairFor("pelvis");
    expect(fabsf(glm_quat_dot(corrections[hand], corrections[fore_arm]))).toBe(1.0f, 1e-6f);
    expect(fabsf(glm_quat_dot(corrections[spine], corrections[parent]))).toBe(1.0f, 1e-6f);
    expect(corrections[pelvis][3]).toBe(1.0f);
  });
});
