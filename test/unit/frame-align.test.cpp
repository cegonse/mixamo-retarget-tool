#include <cest>
#include <cstdio>
extern "C" {
#include <frame_align.h>
#include <transform.h>
}

struct KnownTransform {
  float angle_degrees;
  float axis[3];
  float scale;
  float translation[3];
};

static vec3 cloud[] = {
  {0.0f, 0.0f, 0.0f}, {1.0f, 0.2f, 0.0f}, {0.0f, 1.5f, 0.3f}, {0.4f, 0.1f, 2.0f},
  {-1.0f, 0.7f, 0.5f}, {0.3f, -0.8f, 1.1f}, {2.0f, 2.0f, -1.0f}};
static const size_t cloud_size = sizeof cloud / sizeof cloud[0];

static GltfDoc *source_doc = nullptr, *destination_doc = nullptr;
static Skeleton *source = nullptr, *destination = nullptr;
static BoneMap *map = nullptr;

static void applyKnown(const KnownTransform &known, vec3 *dest) {
  versor rotation;
  vec3 axis = {known.axis[0], known.axis[1], known.axis[2]};
  glm_quatv(rotation, glm_rad(known.angle_degrees), axis);
  for (size_t point = 0; point < cloud_size; point++) {
    glm_quat_rotatev(rotation, cloud[point], dest[point]);
    glm_vec3_scale(dest[point], known.scale, dest[point]);
    glm_vec3_add(dest[point], (float *)known.translation, dest[point]);
  }
}

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

describe("FrameAlign", []() {
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

  it("recovers known similarity transforms", []() {
    cest::withParameter<KnownTransform>()
      .withValue(KnownTransform{0.0f, {1, 0, 0}, 1.0f, {0, 0, 0}})
      .withValue(KnownTransform{90.0f, {1, 0, 0}, 1.0f, {0, 0, 0}})
      .withValue(KnownTransform{90.0f, {0, 1, 0}, 2.0f, {1, 2, 3}})
      .withValue(KnownTransform{90.0f, {0, 0, 1}, 1.0f, {-5, 0, 0}})
      .withValue(KnownTransform{-90.0f, {1, 0, 0}, 0.01f, {0, 0, 0.5f}})
      .withValue(KnownTransform{170.0f, {0.3f, -0.5f, 0.8f}, 450.0f, {-5.5f, -10.8f, -412.7f}})
      .thenDo([](KnownTransform known) {
        vec3 transformed[7];
        versor expected;
        FrameAlignment alignment;
        glm_quatv(expected, glm_rad(known.angle_degrees), (vec3){known.axis[0], known.axis[1], known.axis[2]});
        applyKnown(known, transformed);
        FrameAlign_Solve(cloud, transformed, cloud_size, &alignment);
        expect(alignment.degenerate).toBe(0);
        expect(fabsf(glm_quat_dot(alignment.rotation, expected))).toBe(1.0f, 1e-5f);
        expect(alignment.scale).toBe(known.scale, known.scale * 1e-4f);
        for (int axis = 0; axis < 3; axis++) {
          expect(alignment.translation[axis]).toBe(known.translation[axis], 1e-2f);
        }
        expect(alignment.rms_residual).toBeLessThan(1e-3f * known.scale);
      });
  });

  it("falls back to identity for fewer than three points", []() {
    vec3 two[] = {{0, 0, 0}, {1, 0, 0}};
    vec3 moved[] = {{0, 5, 0}, {0, 6, 0}};
    FrameAlignment alignment;
    FrameAlign_Solve(two, moved, 2, &alignment);
    expect(alignment.degenerate).toBe(1);
    expect(alignment.rotation[3]).toBe(1.0f);
    expect(alignment.scale).toBe(1.0f);
  });

  it("falls back to identity for collinear points", []() {
    vec3 line[] = {{0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {-3, -3, -3}};
    vec3 moved[] = {{0, 0, 0}, {0, 1, 0}, {0, 2, 0}, {0, -3, 0}};
    FrameAlignment alignment;
    FrameAlign_Solve(line, moved, 4, &alignment);
    expect(alignment.degenerate).toBe(1);
    expect(alignment.rotation[3]).toBe(1.0f);
  });

  it("aligns a rig with itself to identity", []() {
    FrameAlignment alignment;
    loadRigs(FIXTURES_DIR "/test_player.glb", FIXTURES_DIR "/test_player.glb",
      DOCS_DIR "/mappings/mixamo-identity.map");
    expect((int)FrameAlign_FromMap(map, source, destination, &alignment)).toBe((int)ERR_NONE);
    expect(alignment.rotation[3]).toBe(1.0f, 1e-5f);
    expect(alignment.scale).toBe(1.0f, 1e-4f);
    expect(alignment.rms_residual).toBeLessThan(1e-2f);
  });

  it("maps the UAL rig's +Y up onto test_player's -Z up", []() {
    FrameAlignment alignment;
    vec3 up = {0.0f, 1.0f, 0.0f}, mapped_up;
    loadRigs(FIXTURES_DIR "/UAL1_Standard_RM.glb", FIXTURES_DIR "/test_player.glb",
      DOCS_DIR "/mappings/ual-to-mixamo.map");
    FrameAlign_FromMap(map, source, destination, &alignment);
    glm_quat_rotatev(alignment.rotation, up, mapped_up);
    expect(mapped_up[2]).toBeLessThan(-0.95f);
    expect(alignment.scale).toBeInRange(300.0f, 500.0f);
    expect(alignment.rms_residual).toBeLessThan(0.2f * alignment.destination_spread);
    expect(mapped_up[1]).toBe(0.0f, 0.05f);
  });
});
