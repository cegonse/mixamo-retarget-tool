#include <frame_align.h>
#include <jacobi.h>
#include <math.h>

typedef struct Centred {
  double source_centroid[3];
  double destination_centroid[3];
  double covariance[3][3];
  double source_spread;
} Centred;

static const double collinear_tolerance = 1e-6;

void FrameAlign_Identity(FrameAlignment *dest) {
  glm_quat_identity(dest->rotation);
  dest->scale = 1.0f;
  glm_vec3_zero(dest->translation);
  dest->rms_residual = 0.0f;
  dest->destination_spread = 0.0f;
  dest->degenerate = 0;
}

static void centroid(vec3 *points, size_t count, double dest[3]) {
  size_t point;
  int axis;
  for (axis = 0; axis < 3; axis++) {
    dest[axis] = 0.0;
    for (point = 0; point < count; point++) {
      dest[axis] += points[point][axis];
    }
    dest[axis] /= (double)count;
  }
}

static void centre(vec3 *source, vec3 *destination, size_t count, Centred *dest) {
  size_t point;
  int a, b;
  centroid(source, count, dest->source_centroid);
  centroid(destination, count, dest->destination_centroid);
  dest->source_spread = 0.0;
  for (a = 0; a < 3; a++) {
    for (b = 0; b < 3; b++) {
      dest->covariance[a][b] = 0.0;
    }
  }
  for (point = 0; point < count; point++) {
    for (a = 0; a < 3; a++) {
      double source_value = source[point][a] - dest->source_centroid[a];
      dest->source_spread += source_value * source_value;
      for (b = 0; b < 3; b++) {
        dest->covariance[a][b] += source_value * (destination[point][b] - dest->destination_centroid[b]);
      }
    }
  }
}

static void hornMatrix(double s[3][3], double n[4][4]) {
  double trace = s[0][0] + s[1][1] + s[2][2];
  double yz = s[1][2] - s[2][1], zx = s[2][0] - s[0][2], xy = s[0][1] - s[1][0];
  n[0][0] = trace;
  n[0][1] = n[1][0] = yz;
  n[0][2] = n[2][0] = zx;
  n[0][3] = n[3][0] = xy;
  n[1][1] = s[0][0] - s[1][1] - s[2][2];
  n[1][2] = n[2][1] = s[0][1] + s[1][0];
  n[1][3] = n[3][1] = s[2][0] + s[0][2];
  n[2][2] = -s[0][0] + s[1][1] - s[2][2];
  n[2][3] = n[3][2] = s[1][2] + s[2][1];
  n[3][3] = -s[0][0] - s[1][1] + s[2][2];
}

static void dominantQuaternion(double covariance[3][3], versor dest) {
  double n[4][4], values[4], vectors[4][4];
  int best = 0, index;
  hornMatrix(covariance, n);
  Jacobi_SymmetricEigen4(n, values, vectors);
  for (index = 1; index < 4; index++) {
    if (values[index] > values[best]) {
      best = index;
    }
  }
  glm_quat_init(dest, (float)vectors[1][best], (float)vectors[2][best], (float)vectors[3][best],
    (float)vectors[0][best]);
  glm_quat_normalize(dest);
  if (dest[3] < 0.0f) {
    glm_vec4_negate(dest);
  }
}

static int isCollinear(vec3 *points, size_t count, const double centre_point[3]) {
  double farthest[3] = {0.0, 0.0, 0.0}, best = 0.0, largest_cross = 0.0;
  size_t point;
  for (point = 0; point < count; point++) {
    double d[3] = {points[point][0] - centre_point[0], points[point][1] - centre_point[1],
      points[point][2] - centre_point[2]};
    double length = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (length > best) {
      best = length;
      farthest[0] = d[0], farthest[1] = d[1], farthest[2] = d[2];
    }
  }
  for (point = 0; point < count && best > 0.0; point++) {
    double d[3] = {points[point][0] - centre_point[0], points[point][1] - centre_point[1],
      points[point][2] - centre_point[2]};
    double cx = d[1] * farthest[2] - d[2] * farthest[1], cy = d[2] * farthest[0] - d[0] * farthest[2];
    double cz = d[0] * farthest[1] - d[1] * farthest[0];
    largest_cross = fmax(largest_cross, sqrt(cx * cx + cy * cy + cz * cz));
  }
  return largest_cross <= collinear_tolerance * best;
}

static float uniformScale(vec3 *source, vec3 *destination, size_t count, const Centred *centred,
    versor rotation) {
  double numerator = 0.0;
  size_t point;
  int axis;
  for (point = 0; point < count; point++) {
    vec3 offset, rotated;
    for (axis = 0; axis < 3; axis++) {
      offset[axis] = (float)(source[point][axis] - centred->source_centroid[axis]);
    }
    glm_quat_rotatev(rotation, offset, rotated);
    for (axis = 0; axis < 3; axis++) {
      numerator += rotated[axis] * (destination[point][axis] - centred->destination_centroid[axis]);
    }
  }
  return (float)(numerator / centred->source_spread);
}

static void residuals(vec3 *source, vec3 *destination, size_t count, FrameAlignment *dest,
    const Centred *centred) {
  double squared = 0.0, spread = 0.0;
  size_t point;
  int axis;
  for (point = 0; point < count; point++) {
    vec3 mapped;
    glm_quat_rotatev(dest->rotation, source[point], mapped);
    glm_vec3_scale(mapped, dest->scale, mapped);
    glm_vec3_add(mapped, dest->translation, mapped);
    for (axis = 0; axis < 3; axis++) {
      double error = mapped[axis] - destination[point][axis];
      double offset = destination[point][axis] - centred->destination_centroid[axis];
      squared += error * error;
      spread += offset * offset;
    }
  }
  dest->rms_residual = (float)sqrt(squared / (double)count);
  dest->destination_spread = (float)sqrt(spread / (double)count);
}

static void solveTranslation(const Centred *centred, FrameAlignment *dest) {
  vec3 source_centroid, mapped;
  int axis;
  for (axis = 0; axis < 3; axis++) {
    source_centroid[axis] = (float)centred->source_centroid[axis];
  }
  glm_quat_rotatev(dest->rotation, source_centroid, mapped);
  for (axis = 0; axis < 3; axis++) {
    dest->translation[axis] = (float)centred->destination_centroid[axis] - dest->scale * mapped[axis];
  }
}

void FrameAlign_Solve(vec3 *source_points, vec3 *destination_points, size_t count,
    FrameAlignment *dest) {
  Centred centred;
  FrameAlign_Identity(dest);
  if (count == 0) {
    dest->degenerate = 1;
    return;
  }
  centre(source_points, destination_points, count, &centred);
  if (count < 3 || isCollinear(source_points, count, centred.source_centroid)
      || isCollinear(destination_points, count, centred.destination_centroid)) {
    dest->degenerate = 1;
  } else {
    dominantQuaternion(centred.covariance, dest->rotation);
    dest->scale = uniformScale(source_points, destination_points, count, &centred, dest->rotation);
  }
  solveTranslation(&centred, dest);
  residuals(source_points, destination_points, count, dest, &centred);
}
