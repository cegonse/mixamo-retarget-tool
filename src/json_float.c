#include <json_float.h>
#include <math.h>
#include <stdio.h>

json_object *JsonFloat_New(float value) {
  char text[32];
  if (!isfinite(value)) {
    return NULL;
  }
  snprintf(text, sizeof text, "%.9g", (double)value);
  return json_object_new_double_s((double)value, text);
}
