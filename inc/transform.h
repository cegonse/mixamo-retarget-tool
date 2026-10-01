#pragma once
#include <cglm/cglm.h>
#include <error_code.h>

typedef struct Transform {
  vec3 translation;
  versor rotation;
  vec3 scale;
} Transform;

void Transform_Identity(Transform *dest);
void Transform_Compose(const Transform *parent, const Transform *child, Transform *dest);
void Transform_Inverse(const Transform *transform, Transform *dest);
void Transform_ToMat4(const Transform *transform, mat4 dest);
ErrorCode Transform_FromMat4(mat4 matrix, Transform *dest);
void Transform_TransformPoint(const Transform *transform, vec3 point, vec3 dest);
void Transform_MinimalArc(vec3 from, vec3 to, versor dest);
void Transform_EulerDegrees(versor rotation, vec3 dest);
void Transform_QuatMakeContinuous(versor previous, versor current);
void Transform_QuatSlerp(versor from, versor to, float amount, versor dest);
