#include <cest>
#include <cmath>
#include <string>
extern "C" {
#include <json_float.h>
}

static std::string serialise(float value) {
  json_object *number = JsonFloat_New(value);
  std::string text = json_object_to_json_string_ext(number, JSON_C_TO_STRING_PLAIN);
  json_object_put(number);
  return text;
}

describe("JsonFloat", []() {
  it("writes floats with %.9g so they round-trip", []() {
    expect(serialise(0.1f)).toBe("0.100000001");
    expect(std::stof(serialise(0.1f))).toBe(0.1f, 0.0f);
    expect(serialise(-412.68457f)).toBe("-412.68457");
  });

  it("writes whole numbers without a decimal point", []() {
    expect(serialise(0.0f)).toBe("0");
    expect(serialise(2.0f)).toBe("2");
  });

  it("refuses non-finite values", []() {
    expect(JsonFloat_New(NAN)).toBeNull();
    expect(JsonFloat_New(INFINITY)).toBeNull();
  });
});
