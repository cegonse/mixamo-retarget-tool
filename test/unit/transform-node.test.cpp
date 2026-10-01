#include <cest>
#include <cstring>
extern "C" {
#include <transform_node.h>
}

describe("Transform_FromNode", []() {
  it("uses spec defaults for a node without TRS", []() {
    cgltf_node node;
    Transform rest;
    memset(&node, 0, sizeof node);
    expect((int)Transform_FromNode(&node, &rest)).toBe((int)ERR_NONE);
    expect(rest.translation[0]).toBe(0.0f);
    expect(rest.rotation[3]).toBe(1.0f);
    expect(rest.scale[2]).toBe(1.0f);
  });

  it("copies TRS fields", []() {
    cgltf_node node;
    Transform rest;
    memset(&node, 0, sizeof node);
    node.has_translation = 1;
    node.translation[1] = 4.0f;
    node.has_rotation = 1;
    node.rotation[0] = -0.70710678f;
    node.rotation[3] = 0.70710678f;
    expect((int)Transform_FromNode(&node, &rest)).toBe((int)ERR_NONE);
    expect(rest.translation[1]).toBe(4.0f);
    expect(rest.rotation[0]).toBe(-0.70710678f, 1e-6f);
    expect(rest.scale[0]).toBe(1.0f);
  });

  it("decomposes a matrix node", []() {
    cgltf_node node;
    Transform expected, rest;
    mat4 matrix;
    memset(&node, 0, sizeof node);
    Transform_Identity(&expected);
    glm_quatv(expected.rotation, glm_rad(-90.0f), (vec3){1.0f, 0.0f, 0.0f});
    glm_vec3_copy((vec3){-5.0f, -10.0f, -412.0f}, expected.translation);
    glm_vec3_fill(expected.scale, 2.0f);
    Transform_ToMat4(&expected, matrix);
    node.has_matrix = 1;
    memcpy(node.matrix, matrix, sizeof matrix);
    expect((int)Transform_FromNode(&node, &rest)).toBe((int)ERR_NONE);
    expect(rest.translation[2]).toBe(-412.0f, 1e-4f);
    expect(fabsf(glm_quat_dot(rest.rotation, expected.rotation))).toBe(1.0f, 1e-5f);
    expect(rest.scale[1]).toBe(2.0f, 1e-5f);
  });

  it("rejects a sheared matrix node", []() {
    cgltf_node node;
    Transform rest;
    mat4 matrix = GLM_MAT4_IDENTITY_INIT;
    memset(&node, 0, sizeof node);
    matrix[2][0] = 0.3f;
    node.has_matrix = 1;
    memcpy(node.matrix, matrix, sizeof matrix);
    expect((int)Transform_FromNode(&node, &rest)).toBe((int)ERR_BAD_GLB);
  });
});
