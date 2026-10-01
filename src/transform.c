#include <transform.h>

static const float orthogonality_tolerance = 1e-4f;
static const float minimum_axis_length = 1e-8f;

void Transform_Identity(Transform *dest) {
  glm_vec3_zero(dest->translation);
  glm_quat_identity(dest->rotation);
  glm_vec3_one(dest->scale);
}

void Transform_TransformPoint(const Transform *transform, vec3 point, vec3 dest) {
  Transform local = *transform;
  vec3 scaled;
  glm_vec3_mul(local.scale, point, scaled);
  glm_quat_rotatev(local.rotation, scaled, dest);
  glm_vec3_add(dest, local.translation, dest);
}

void Transform_Compose(const Transform *parent, const Transform *child, Transform *dest) {
  Transform parent_copy = *parent;
  Transform child_copy = *child;
  Transform_TransformPoint(&parent_copy, child_copy.translation, dest->translation);
  glm_quat_mul(parent_copy.rotation, child_copy.rotation, dest->rotation);
  glm_quat_normalize(dest->rotation);
  glm_vec3_mul(parent_copy.scale, child_copy.scale, dest->scale);
}

void Transform_Inverse(const Transform *transform, Transform *dest) {
  Transform source = *transform;
  vec3 negated_translation;
  glm_vec3_div((vec3){1.0f, 1.0f, 1.0f}, source.scale, dest->scale);
  glm_quat_inv(source.rotation, dest->rotation);
  glm_vec3_negate_to(source.translation, negated_translation);
  glm_quat_rotatev(dest->rotation, negated_translation, dest->translation);
  glm_vec3_mul(dest->scale, dest->translation, dest->translation);
}

void Transform_ToMat4(const Transform *transform, mat4 dest) {
  Transform local = *transform;
  glm_translate_make(dest, local.translation);
  glm_quat_rotate(dest, local.rotation, dest);
  glm_scale(dest, local.scale);
}

static int hasDegenerateAxis(vec3 scale) {
  return fabsf(scale[0]) < minimum_axis_length || fabsf(scale[1]) < minimum_axis_length
    || fabsf(scale[2]) < minimum_axis_length;
}

static int isAffine(mat4 matrix) {
  return matrix[0][3] == 0.0f && matrix[1][3] == 0.0f && matrix[2][3] == 0.0f
    && matrix[3][3] == 1.0f;
}

static int hasShear(mat4 rotation) {
  return fabsf(glm_vec3_dot(rotation[0], rotation[1])) > orthogonality_tolerance
    || fabsf(glm_vec3_dot(rotation[0], rotation[2])) > orthogonality_tolerance
    || fabsf(glm_vec3_dot(rotation[1], rotation[2])) > orthogonality_tolerance;
}

ErrorCode Transform_FromMat4(mat4 matrix, Transform *dest) {
  vec4 translation;
  mat4 rotation;
  vec3 scale;
  if (!isAffine(matrix)) {
    return ERR_BAD_GLB;
  }
  glm_decompose(matrix, translation, rotation, scale);
  if (hasDegenerateAxis(scale) || hasShear(rotation)) {
    return ERR_BAD_GLB;
  }
  glm_vec3_copy(translation, dest->translation);
  glm_mat4_quat(rotation, dest->rotation);
  glm_quat_normalize(dest->rotation);
  glm_vec3_copy(scale, dest->scale);
  return ERR_NONE;
}

void Transform_MinimalArc(vec3 from, vec3 to, versor dest) {
  vec3 from_unit, to_unit;
  if (glm_vec3_norm(from) < minimum_axis_length || glm_vec3_norm(to) < minimum_axis_length) {
    glm_quat_identity(dest);
    return;
  }
  glm_vec3_normalize_to(from, from_unit);
  glm_vec3_normalize_to(to, to_unit);
  glm_quat_from_vecs(from_unit, to_unit, dest);
}

void Transform_EulerDegrees(versor rotation, vec3 dest) {
  mat4 matrix;
  glm_quat_mat4(rotation, matrix);
  glm_euler_angles(matrix, dest);
  glm_vec3_scale(dest, 180.0f / GLM_PIf, dest);
}

void Transform_QuatMakeContinuous(versor previous, versor current) {
  if (glm_quat_dot(previous, current) < 0.0f) {
    glm_vec4_negate(current);
  }
}

void Transform_QuatSlerp(versor from, versor to, float amount, versor dest) {
  versor continuous_to;
  glm_quat_copy(to, continuous_to);
  Transform_QuatMakeContinuous(from, continuous_to);
  glm_quat_slerp(from, continuous_to, amount, dest);
  glm_quat_normalize(dest);
}
