#include <cest>
#include <string>
extern "C" {
#include <app_main.h>
#include <error_code.h>
#include <cglm/cglm.h>
#include <json.h>
}

static std::string readStream(FILE *stream) {
  std::string text;
  char buffer[256];
  rewind(stream);
  while (fgets(buffer, sizeof buffer, stream) != NULL) {
    text += buffer;
  }
  return text;
}

static FILE *captured = nullptr;

describe("App", []() {
  beforeEach([&]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
  });

  afterEach([&]() {
    App_SetOutputStream(NULL);
    fclose(captured);
  });

  it("returns ERR_BAD_ARGS with no arguments", [&]() {
    char program[] = "anim-retarget";
    char *argv[] = {program, NULL};
    expect(App_Run(1, argv)).toBe((int)ERR_BAD_ARGS);
  });

  it("prints usage and succeeds with --help", [&]() {
    char program[] = "anim-retarget";
    char help[] = "--help";
    char *argv[] = {program, help, NULL};
    expect(App_Run(2, argv)).toBe((int)ERR_NONE);
    expect(readStream(captured)).toMatch("usage:");
  });

  it("links cglm", []() {
    versor rotation;
    glm_quat_identity(rotation);
    expect(rotation[3]).toBe(1.0f, 1e-6f);
    expect(rotation[0]).toBe(0.0f, 1e-6f);
  });

  it("links json-c", []() {
    json_object *root = json_object_new_object();
    expect(root).toBeNotNull();
    json_object_object_add(root, "version", json_object_new_string("2.0"));
    expect(std::string(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN)))
      .toBe("{\"version\":\"2.0\"}");
    json_object_put(root);
  });
});
