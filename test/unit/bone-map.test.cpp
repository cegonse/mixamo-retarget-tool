#include <cest>
#include <cstdio>
#include <string>
extern "C" {
#include <bone_map.h>
}

static BoneMap *map = nullptr;
static GltfDoc *player_doc = nullptr;
static GltfDoc *library_doc = nullptr;
static Skeleton *player = nullptr;
static Skeleton *library = nullptr;

static std::string writeMapFile(const char *contents) {
  std::string path = std::string(TEST_OUTPUT_DIR) + "/bone-map-test.map";
  FILE *file = fopen(path.c_str(), "w");
  fputs(contents, file);
  fclose(file);
  return path;
}

static Skeleton *loadSkeleton(const char *path, GltfDoc **doc) {
  ErrorCode error;
  *doc = GltfDoc_Load(path, &error);
  return Skeleton_FromDoc(*doc, 0, &error);
}

static size_t pairNamed(const char *source_name) {
  for (size_t pair = 0; pair < BoneMap_PairCount(map); pair++) {
    if (std::string(BoneMap_SourceName(map, pair)) == source_name) {
      return pair;
    }
  }
  return BONE_MAP_NO_PAIR;
}

describe("BoneMap", []() {
  beforeEach([]() {
    map = BoneMap_Create();
  });

  afterEach([]() {
    BoneMap_Destroy(map);
    Skeleton_Destroy(player);
    Skeleton_Destroy(library);
    GltfDoc_Destroy(player_doc);
    GltfDoc_Destroy(library_doc);
    map = nullptr;
    player = library = nullptr;
    player_doc = library_doc = nullptr;
  });

  it("parses comma-separated pairs with colons in names", []() {
    expect((int)BoneMap_AddPairs(map, "pelvis=mixamorig:Hips, spine_01 = mixamorig:Spine")).toBe((int)ERR_NONE);
    expect(BoneMap_PairCount(map)).toBe((size_t)2);
    expect(std::string(BoneMap_SourceName(map, 0))).toBe("pelvis");
    expect(std::string(BoneMap_DestinationName(map, 0))).toBe("mixamorig:Hips");
    expect(std::string(BoneMap_SourceName(map, 1))).toBe("spine_01");
    expect(std::string(BoneMap_DestinationName(map, 1))).toBe("mixamorig:Spine");
  });

  it("lets a later pair win for the same source bone", []() {
    BoneMap_AddPairs(map, "a=b");
    BoneMap_AddPairs(map, "c=d,a=e");
    expect(BoneMap_PairCount(map)).toBe((size_t)2);
    expect(std::string(BoneMap_DestinationName(map, 0))).toBe("e");
  });

  it("rejects an entry without '=' naming it", []() {
    expect((int)BoneMap_AddPairs(map, "a=b,pelvis")).toBe((int)ERR_BAD_ARGS);
    expect(std::string(BoneMap_ErrorMessage(map))).toMatch("\"pelvis\"");
  });

  it("rejects an empty bone name", []() {
    expect((int)BoneMap_AddPairs(map, "=mixamorig:Hips")).toBe((int)ERR_BAD_ARGS);
    expect((int)BoneMap_AddPairs(map, "pelvis=")).toBe((int)ERR_BAD_ARGS);
  });

  it("reads map files with comments and blank lines", []() {
    std::string path = writeMapFile("# header\n\npelvis=mixamorig:Hips  # trailing\n  \nHead=mixamorig:Head\n");
    expect((int)BoneMap_AddFile(map, path.c_str())).toBe((int)ERR_NONE);
    expect(BoneMap_PairCount(map)).toBe((size_t)2);
    expect(std::string(BoneMap_DestinationName(map, 0))).toBe("mixamorig:Hips");
    expect(std::string(BoneMap_SourceName(map, 1))).toBe("Head");
    remove(path.c_str());
  });

  it("reports a missing map file as ERR_OPEN_INPUT", []() {
    expect((int)BoneMap_AddFile(map, FIXTURES_DIR "/missing.map")).toBe((int)ERR_OPEN_INPUT);
  });

  it("resolves the identity map on the Mixamo rig", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    expect((int)BoneMap_AddFile(map, DOCS_DIR "/mappings/mixamo-identity.map")).toBe((int)ERR_NONE);
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_NONE);
    expect(BoneMap_PairCount(map)).toBe((size_t)25);
    for (size_t joint = 0; joint < Skeleton_JointCount(player); joint++) {
      size_t pair = BoneMap_PairForDestination(map, joint);
      expect(pair).Not->toBe(BONE_MAP_NO_PAIR);
      expect(BoneMap_SourceJoint(map, pair)).toBe(joint);
    }
  });

  it("finds the root pair and mapped children", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    BoneMap_AddFile(map, DOCS_DIR "/mappings/mixamo-identity.map");
    BoneMap_Resolve(map, player, player);
    size_t hips = pairNamed("mixamorig:Hips");
    size_t spine2 = pairNamed("mixamorig:Spine2");
    size_t fore_arm = pairNamed("mixamorig:LeftForeArm");
    expect(BoneMap_IsRootPair(map, hips)).toBeTruthy();
    expect(BoneMap_IsRootPair(map, spine2)).toBeFalsy();
    expect(BoneMap_MappedChildCount(map, hips)).toBe((size_t)3);
    expect(BoneMap_MappedChildCount(map, spine2)).toBe((size_t)3);
    expect(BoneMap_MappedChildCount(map, fore_arm)).toBe((size_t)1);
    expect(BoneMap_MappedChild(map, fore_arm, 0)).toBe(pairNamed("mixamorig:LeftHand"));
    expect(BoneMap_MappedParent(map, fore_arm)).toBe(pairNamed("mixamorig:LeftArm"));
  });

  it("skips unmapped destination joints when finding mapped parents", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    BoneMap_AddPairs(map, "mixamorig:Hips=mixamorig:Hips,mixamorig:Neck=mixamorig:Neck");
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_NONE);
    expect(BoneMap_MappedParent(map, 1)).toBe((size_t)0);
    expect(BoneMap_MappedChildCount(map, 0)).toBe((size_t)1);
  });

  it("resolves the UAL map onto the Mixamo rig with root unmapped", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    library = loadSkeleton(FIXTURES_DIR "/UAL1_Standard_RM.glb", &library_doc);
    BoneMap_AddFile(map, DOCS_DIR "/mappings/ual-to-mixamo.map");
    expect((int)BoneMap_Resolve(map, library, player)).toBe((int)ERR_NONE);
    expect(BoneMap_PairCount(map)).toBe((size_t)24);
    expect(BoneMap_PairForSource(map, Skeleton_FindJoint(library, "root"))).toBe(BONE_MAP_NO_PAIR);
    expect(BoneMap_IsRootPair(map, pairNamed("pelvis"))).toBeTruthy();
    expect(BoneMap_PairForDestination(map, Skeleton_FindJoint(player, "mixamorig:HeadTop_End")))
      .toBe(BONE_MAP_NO_PAIR);
  });

  it("names an unknown source bone and lists what is available", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    BoneMap_AddPairs(map, "Pelvis=mixamorig:Hips");
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_NOT_FOUND);
    std::string message = BoneMap_ErrorMessage(map);
    expect(message).toMatch("unknown source bone \"Pelvis\"");
    expect(message).toMatch("available: mixamorig:Hips, mixamorig:Spine");
  });

  it("names an unknown destination bone", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    BoneMap_AddPairs(map, "mixamorig:Hips=Hips");
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_NOT_FOUND);
    expect(std::string(BoneMap_ErrorMessage(map))).toMatch("unknown destination bone \"Hips\"");
  });

  it("rejects a destination bone mapped twice", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    BoneMap_AddPairs(map, "mixamorig:Hips=mixamorig:Hips,mixamorig:Spine=mixamorig:Hips");
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_BAD_ARGS);
    expect(std::string(BoneMap_ErrorMessage(map))).toMatch("\"mixamorig:Hips\" is mapped twice");
  });

  it("rejects an empty map", []() {
    player = loadSkeleton(FIXTURES_DIR "/test_player.glb", &player_doc);
    expect((int)BoneMap_Resolve(map, player, player)).toBe((int)ERR_BAD_ARGS);
  });

  it("destroys NULL harmlessly", []() {
    BoneMap_Destroy(NULL);
  });
});
