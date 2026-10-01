#include <cest>
extern "C" {
#include <transform.h>
}

static void makeTransform(Transform *dest, float angle_degrees, float axis_x, float axis_y,
    float axis_z, float scale) {
  vec3 axis = {axis_x, axis_y, axis_z};
  glm_quatv(dest->rotation, glm_rad(angle_degrees), axis);
  glm_vec3_copy((vec3){1.0f, 2.0f, 3.0f}, dest->translation);
  glm_vec3_fill(dest->scale, scale);
}

static void expectVec3(const float *actual, float x, float y, float z, float epsilon) {
  expect(actual[0]).toBe(x, epsilon);
  expect(actual[1]).toBe(y, epsilon);
  expect(actual[2]).toBe(z, epsilon);
}

static void expectSameRotation(versor actual, versor expected, float epsilon) {
  expect(fabsf(glm_quat_dot(actual, expected))).toBe(1.0f, epsilon);
}

static void expectIdentity(const Transform *transform, float epsilon) {
  versor identity;
  glm_quat_identity(identity);
  expectVec3(transform->translation, 0.0f, 0.0f, 0.0f, epsilon);
  expectSameRotation((float *)transform->rotation, identity, epsilon);
  expectVec3(transform->scale, 1.0f, 1.0f, 1.0f, epsilon);
}

static void expectRotatesOnto(vec3 from, vec3 to, float epsilon) {
  versor rotation;
  vec3 rotated, from_unit, to_unit;
  Transform_MinimalArc(from, to, rotation);
  glm_vec3_normalize_to(from, from_unit);
  glm_vec3_normalize_to(to, to_unit);
  glm_quat_rotatev(rotation, from_unit, rotated);
  expectVec3(rotated, to_unit[0], to_unit[1], to_unit[2], epsilon);
  expect(glm_quat_norm(rotation)).toBe(1.0f, 1e-5f);
}

struct ArcCase {
  float from[3];
  float to[3];
  float epsilon;
};

describe("Transform", []() {
  it("builds the identity", []() {
    Transform identity;
    Transform_Identity(&identity);
    expectIdentity(&identity, 0.0f);
    expect(identity.rotation[3]).toBe(1.0f);
  });

  it("composes with identity to the child", []() {
    Transform identity, child, result;
    Transform_Identity(&identity);
    makeTransform(&child, 30.0f, 0.0f, 1.0f, 0.0f, 2.0f);
    Transform_Compose(&identity, &child, &result);
    expectVec3(result.translation, 1.0f, 2.0f, 3.0f, 1e-6f);
    expectSameRotation(result.rotation, child.rotation, 1e-6f);
    expectVec3(result.scale, 2.0f, 2.0f, 2.0f, 1e-6f);
  });

  it("transforms a point by scale, then rotation, then translation", []() {
    Transform transform;
    vec3 point = {1.0f, 0.0f, 0.0f}, result;
    makeTransform(&transform, 90.0f, 0.0f, 0.0f, 1.0f, 2.0f);
    Transform_TransformPoint(&transform, point, result);
    expectVec3(result, 1.0f, 4.0f, 3.0f, 1e-5f);
  });

  it("composes like the matrix product", []() {
    Transform parent, child, composed;
    mat4 parent_matrix, child_matrix, product, composed_matrix;
    makeTransform(&parent, 90.0f, 1.0f, 0.0f, 0.0f, 2.0f);
    makeTransform(&child, -40.0f, 0.0f, 1.0f, 1.0f, 0.5f);
    Transform_Compose(&parent, &child, &composed);
    Transform_ToMat4(&parent, parent_matrix);
    Transform_ToMat4(&child, child_matrix);
    glm_mat4_mul(parent_matrix, child_matrix, product);
    Transform_ToMat4(&composed, composed_matrix);
    for (int column = 0; column < 4; column++) {
      for (int row = 0; row < 4; row++) {
        expect(composed_matrix[column][row]).toBe(product[column][row], 1e-5f);
      }
    }
  });

  it("round-trips compose with inverse to identity", []() {
    Transform transform, inverse, result;
    makeTransform(&transform, 90.0f, 0.0f, 1.0f, 0.0f, 2.0f);
    Transform_Inverse(&transform, &inverse);
    Transform_Compose(&transform, &inverse, &result);
    expectIdentity(&result, 1e-5f);
    Transform_Compose(&inverse, &transform, &result);
    expectIdentity(&result, 1e-5f);
  });

  it("composes in place when dest aliases an input", []() {
    Transform transform, inverse;
    makeTransform(&transform, 60.0f, 1.0f, 1.0f, 0.0f, 3.0f);
    Transform_Inverse(&transform, &inverse);
    Transform_Compose(&transform, &inverse, &transform);
    expectIdentity(&transform, 1e-5f);
  });

  it("recovers TRS from its matrix", []() {
    Transform original, recovered;
    mat4 matrix;
    makeTransform(&original, -90.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    original.scale[1] = 3.0f;
    Transform_ToMat4(&original, matrix);
    expect((int)Transform_FromMat4(matrix, &recovered)).toBe((int)ERR_NONE);
    expectVec3(recovered.translation, 1.0f, 2.0f, 3.0f, 1e-5f);
    expectSameRotation(recovered.rotation, original.rotation, 1e-5f);
    expectVec3(recovered.scale, 1.0f, 3.0f, 1.0f, 1e-5f);
  });

  it("rejects a sheared matrix", []() {
    Transform recovered;
    mat4 matrix = GLM_MAT4_IDENTITY_INIT;
    matrix[1][0] = 0.5f;
    expect((int)Transform_FromMat4(matrix, &recovered)).toBe((int)ERR_BAD_GLB);
  });

  it("rejects a matrix with a zero-length axis", []() {
    Transform recovered;
    mat4 matrix = GLM_MAT4_IDENTITY_INIT;
    matrix[2][2] = 0.0f;
    expect((int)Transform_FromMat4(matrix, &recovered)).toBe((int)ERR_BAD_GLB);
  });

  it("finds the minimal arc between directions", []() {
    cest::withParameter<ArcCase>()
      .withValue(ArcCase{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 1e-5f})
      .withValue(ArcCase{{0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, -5.0f}, 1e-5f})
      .withValue(ArcCase{{1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 1e-5f})
      .withValue(ArcCase{{0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 1e-5f})
      .withValue(ArcCase{{1.0f, 0.0f, 0.0f}, {1.0f, 1e-4f, 0.0f}, 5e-4f})
      .withValue(ArcCase{{0.3f, -0.4f, 0.5f}, {-0.7f, 0.1f, 0.2f}, 1e-5f})
      .thenDo([](ArcCase arc) { expectRotatesOnto(arc.from, arc.to, arc.epsilon); });
  });

  it("returns identity for a zero-length direction", []() {
    vec3 zero = {0.0f, 0.0f, 0.0f}, up = {0.0f, 1.0f, 0.0f};
    versor rotation;
    Transform_MinimalArc(zero, up, rotation);
    expectVec3(rotation, 0.0f, 0.0f, 0.0f, 0.0f);
    expect(rotation[3]).toBe(1.0f);
  });

  it("reads a 90 degree X rotation as Euler (90, 0, 0)", []() {
    versor rotation;
    vec3 degrees;
    glm_quatv(rotation, glm_rad(90.0f), (vec3){1.0f, 0.0f, 0.0f});
    Transform_EulerDegrees(rotation, degrees);
    expectVec3(degrees, 90.0f, 0.0f, 0.0f, 1e-3f);
  });

  it("flips a quaternion that points away from the previous one", []() {
    versor previous = {0.0f, 0.0f, 0.0f, 1.0f};
    versor current = {0.0f, 0.1f, 0.0f, -0.995f};
    Transform_QuatMakeContinuous(previous, current);
    expect(current[1]).toBe(-0.1f, 1e-6f);
    expect(current[3]).toBe(0.995f, 1e-6f);
    Transform_QuatMakeContinuous(previous, current);
    expect(current[3]).toBe(0.995f, 1e-6f);
  });

  it("slerps halfway along a 90 degree arc", []() {
    versor from, to, middle, expected;
    vec3 z_axis = {0.0f, 0.0f, 1.0f};
    glm_quat_identity(from);
    glm_quatv(to, glm_rad(90.0f), z_axis);
    glm_quatv(expected, glm_rad(45.0f), z_axis);
    Transform_QuatSlerp(from, to, 0.5f, middle);
    expectSameRotation(middle, expected, 1e-5f);
  });

  it("slerps between opposite-sign equivalent rotations without leaving them", []() {
    versor from, to, middle;
    vec3 y_axis = {0.0f, 1.0f, 0.0f};
    glm_quatv(from, glm_rad(20.0f), y_axis);
    glm_quatv(to, glm_rad(20.05f), y_axis);
    glm_vec4_negate(to);
    Transform_QuatSlerp(from, to, 0.5f, middle);
    expect(glm_quat_norm(middle)).toBe(1.0f, 1e-5f);
    expectSameRotation(middle, from, 1e-5f);
  });

  it("takes the short way where the long way differs", []() {
    versor from, to, shortest, longest;
    vec3 x_axis = {1.0f, 0.0f, 0.0f};
    glm_quat_identity(from);
    glm_quatv(to, glm_rad(270.0f), x_axis);
    Transform_QuatSlerp(from, to, 0.5f, shortest);
    glm_quat_slerp_longest(from, to, 0.5f, longest);
    expect(glm_quat_angle(shortest)).toBe(glm_rad(45.0f), 1e-4f);
    expect(fabsf(glm_quat_dot(shortest, longest))).toBeLessThan(0.9f);
  });
});
