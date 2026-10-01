#include <cest>
extern "C" {
#include <keyframes.h>
#include <transform.h>
}

static const float times[] = {0.0f, 1.0f, 3.0f};
static const float scalar_values[] = {10.0f, 20.0f, 0.0f};

struct Sample {
  float time;
  float expected;
};

static Keyframes scalarTrack(Interpolation interpolation) {
  return Keyframes{interpolation, 3, 1, times, scalar_values};
}

describe("Keyframes", []() {
  it("evaluates STEP at, between and outside keys", []() {
    Keyframes track = scalarTrack(INTERPOLATION_STEP);
    cest::withParameter<Sample>()
      .withValue(Sample{-1.0f, 10.0f})
      .withValue(Sample{0.0f, 10.0f})
      .withValue(Sample{0.99f, 10.0f})
      .withValue(Sample{1.0f, 20.0f})
      .withValue(Sample{2.5f, 20.0f})
      .withValue(Sample{3.0f, 0.0f})
      .withValue(Sample{9.0f, 0.0f})
      .thenDo([&](Sample sample) {
        float value;
        Keyframes_Evaluate(&track, sample.time, &value);
        expect(value).toBe(sample.expected, 1e-6f);
      });
  });

  it("evaluates LINEAR at, between and outside keys", []() {
    Keyframes track = scalarTrack(INTERPOLATION_LINEAR);
    cest::withParameter<Sample>()
      .withValue(Sample{-1.0f, 10.0f})
      .withValue(Sample{0.5f, 15.0f})
      .withValue(Sample{1.0f, 20.0f})
      .withValue(Sample{2.0f, 10.0f})
      .withValue(Sample{3.5f, 0.0f})
      .thenDo([&](Sample sample) {
        float value;
        Keyframes_Evaluate(&track, sample.time, &value);
        expect(value).toBe(sample.expected, 1e-5f);
      });
  });

  it("evaluates CUBICSPLINE with Hermite weights", []() {
    static const float cubic_times[] = {0.0f, 2.0f};
    static const float cubic_values[] = {0.0f, 1.0f, 3.0f, 5.0f, 4.0f, 0.0f};
    Keyframes track = {INTERPOLATION_CUBICSPLINE, 2, 1, cubic_times, cubic_values};
    float value;
    Keyframes_Evaluate(&track, 0.0f, &value);
    expect(value).toBe(1.0f, 1e-6f);
    Keyframes_Evaluate(&track, 2.0f, &value);
    expect(value).toBe(4.0f, 1e-6f);
    Keyframes_Evaluate(&track, 1.0f, &value);
    expect(value).toBe(0.5f * 1.0f + 0.125f * 2.0f * 3.0f + 0.5f * 4.0f - 0.125f * 2.0f * 5.0f, 1e-5f);
  });

  it("slerps rotations along the shortest path", []() {
    static float rotation_values[8];
    static const float rotation_times[] = {0.0f, 1.0f};
    Keyframes track = {INTERPOLATION_LINEAR, 2, 4, rotation_times, rotation_values};
    versor value, expected;
    glm_quat_identity(rotation_values);
    glm_quatv(&rotation_values[4], glm_rad(90.0f), (vec3){0.0f, 1.0f, 0.0f});
    glm_vec4_negate(&rotation_values[4]);
    Keyframes_Evaluate(&track, 0.5f, value);
    glm_quatv(expected, glm_rad(45.0f), (vec3){0.0f, 1.0f, 0.0f});
    expect(fabsf(glm_quat_dot(value, expected))).toBe(1.0f, 1e-5f);
  });

  it("holds a single key", []() {
    static const float single_time[] = {0.5f};
    static const float single_value[] = {7.0f};
    Keyframes track = {INTERPOLATION_LINEAR, 1, 1, single_time, single_value};
    float value;
    Keyframes_Evaluate(&track, 3.0f, &value);
    expect(value).toBe(7.0f);
  });
});
