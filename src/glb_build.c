#include <glb_build.h>
#include <json_float.h>
#include <math.h>

void GlbBuild_AddFloat(GlbBuild *self, json_object *array, float value) {
  json_object *number = JsonFloat_New(value);
  if (number == NULL) {
    self->error = ERR_INTERNAL;
    return;
  }
  json_object_array_add(array, number);
}

json_object *GlbBuild_FloatArray(GlbBuild *self, const float *values, size_t count) {
  json_object *array = json_object_new_array();
  size_t index;
  for (index = 0; index < count; index++) {
    GlbBuild_AddFloat(self, array, values[index]);
  }
  return array;
}

static void addBounds(GlbBuild *self, json_object *accessor, const float *values, size_t count) {
  float minimum = INFINITY, maximum = -INFINITY;
  size_t index;
  for (index = 0; index < count; index++) {
    minimum = fminf(minimum, values[index]);
    maximum = fmaxf(maximum, values[index]);
  }
  json_object_object_add(accessor, "max", GlbBuild_FloatArray(self, &maximum, 1));
  json_object_object_add(accessor, "min", GlbBuild_FloatArray(self, &minimum, 1));
}

static size_t addBufferView(GlbBuild *self, const float *values, size_t float_count) {
  json_object *view = json_object_new_object();
  size_t offset = ByteBuffer_Size(self->bin), index;
  for (index = 0; index < float_count && self->error == ERR_NONE; index++) {
    if (!isfinite(values[index])) {
      self->error = ERR_INTERNAL;
    } else {
      self->error = ByteBuffer_AppendFloat(self->bin, values[index]);
    }
  }
  json_object_object_add(view, "buffer", json_object_new_int(0));
  json_object_object_add(view, "byteLength", json_object_new_int64((int64_t)(float_count * 4)));
  json_object_object_add(view, "byteOffset", json_object_new_int64((int64_t)offset));
  json_object_array_add(self->buffer_views, view);
  return json_object_array_length(self->buffer_views) - 1;
}

size_t GlbBuild_AddAccessor(GlbBuild *self, const float *values, size_t count, const char *type,
    size_t components, int with_bounds) {
  json_object *accessor = json_object_new_object();
  size_t view = addBufferView(self, values, count * components);
  json_object_object_add(accessor, "bufferView", json_object_new_int64((int64_t)view));
  json_object_object_add(accessor, "componentType", json_object_new_int(GLTF_COMPONENT_FLOAT));
  json_object_object_add(accessor, "count", json_object_new_int64((int64_t)count));
  if (with_bounds) {
    addBounds(self, accessor, values, count);
  }
  json_object_object_add(accessor, "type", json_object_new_string(type));
  json_object_array_add(self->accessors, accessor);
  return json_object_array_length(self->accessors) - 1;
}
