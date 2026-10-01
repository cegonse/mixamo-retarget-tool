#include <retarget.h>
#include <retarget_data.h>
#include <rest_align.h>
#include <stdlib.h>

void RetargetOptions_Default(RetargetOptions *dest) {
  dest->frame_align = 1;
  dest->has_frame_rotation = 0;
  glm_vec3_zero(dest->frame_rotation_degrees);
  dest->has_frame_scale = 0;
  dest->frame_scale = 1.0f;
  dest->rest_align = 1;
  dest->in_place = 0;
  glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, dest->source_up);
}

static void rotationFromDegrees(vec3 degrees, versor dest) {
  versor about_x, about_y, about_z, about_zy;
  glm_quatv(about_x, glm_rad(degrees[0]), (vec3){1.0f, 0.0f, 0.0f});
  glm_quatv(about_y, glm_rad(degrees[1]), (vec3){0.0f, 1.0f, 0.0f});
  glm_quatv(about_z, glm_rad(degrees[2]), (vec3){0.0f, 0.0f, 1.0f});
  glm_quat_mul(about_z, about_y, about_zy);
  glm_quat_mul(about_zy, about_x, dest);
}

static ErrorCode solveAlignment(Retarget *self, const RetargetOptions *options) {
  ErrorCode error = ERR_NONE;
  FrameAlign_Identity(&self->alignment);
  if (options->frame_align) {
    error = FrameAlign_FromMap(self->map, self->source, self->destination, &self->alignment);
  }
  if (options->has_frame_rotation) {
    rotationFromDegrees((float *)options->frame_rotation_degrees, self->alignment.rotation);
  }
  if (options->has_frame_scale) {
    self->alignment.scale = options->frame_scale;
  }
  glm_quat_rotatev(self->alignment.rotation, (float *)options->source_up, self->up);
  glm_vec3_normalize(self->up);
  return error;
}

static ErrorCode allocate(Retarget *self) {
  size_t pairs = BoneMap_PairCount(self->map);
  size_t joints = Skeleton_JointCount(self->destination);
  self->corrections = malloc((pairs > 0 ? pairs : 1) * sizeof *self->corrections);
  self->previous_rotations = malloc((joints > 0 ? joints : 1) * sizeof *self->previous_rotations);
  return self->corrections != NULL && self->previous_rotations != NULL ? ERR_NONE : ERR_INTERNAL;
}

Retarget *Retarget_Create(const Skeleton *source, const Skeleton *destination, const BoneMap *map,
    const RetargetOptions *options, ErrorCode *error) {
  Retarget *self = calloc(1, sizeof *self);
  if (self == NULL) {
    *error = ERR_INTERNAL;
    return NULL;
  }
  self->source = source;
  self->destination = destination;
  self->map = map;
  self->in_place = options->in_place;
  *error = allocate(self);
  if (*error == ERR_NONE) {
    *error = solveAlignment(self, options);
  }
  if (*error != ERR_NONE) {
    Retarget_Destroy(self);
    return NULL;
  }
  RestAlign_Identity(BoneMap_PairCount(map), self->corrections);
  if (options->rest_align) {
    RestAlign_Compute(map, source, destination, self->alignment.rotation, self->corrections);
  }
  Retarget_BeginTrack(self);
  return self;
}

void Retarget_Destroy(Retarget *self) {
  if (self == NULL) {
    return;
  }
  free(self->corrections);
  free(self->previous_rotations);
  free(self);
}

const FrameAlignment *Retarget_Alignment(const Retarget *self) {
  return &self->alignment;
}

void Retarget_BeginTrack(Retarget *self) {
  self->has_previous = 0;
}
