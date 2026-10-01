#include <cest>
#include <string>
extern "C" {
#include <app_main.h>
#include <error_code.h>
}

static FILE *captured = nullptr;

static std::string capturedText() {
  std::string text;
  char buffer[512];
  rewind(captured);
  while (fgets(buffer, sizeof buffer, captured) != NULL) {
    text += buffer;
  }
  return text;
}

static int runInfo(const char *path, const char *flag) {
  char program[] = "anim-retarget";
  char command[] = "info";
  char *argv[] = {program, command, (char *)path, (char *)flag, NULL};
  return App_Run(flag != NULL ? 4 : 3, argv);
}

describe("info command", []() {
  beforeEach([]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
  });

  afterEach([]() {
    App_SetOutputStream(NULL);
    fclose(captured);
  });

  it("lists the sword_run.glb track, skin and armature", []() {
    expect(runInfo(FIXTURES_DIR "/sword_run.glb", NULL)).toBe((int)ERR_NONE);
    std::string text = capturedText();
    expect(text).toMatch("asset: glTF 2.0, generator \"Khronos glTF Blender I/O v5.1.19\"\n");
    expect(text).toMatch("animations: 1\n  [0] \"mixamo.com\"  0.700 s  21 keys @ 30 fps  "
      "75 channels (T 25, R 25, S 25)  LINEAR,STEP\n");
    expect(text).toMatch("skins: 1\n  [0] \"Armature\"  25 joints  inverseBindMatrices: yes\n");
    expect(text).toMatch("armature (skin 0, root node 25 \"Armature\"):\n  mixamorig:Hips\n"
      "    mixamorig:Spine\n      mixamorig:Spine1\n");
    expect(text).toMatch("\n    mixamorig:LeftUpLeg\n");
  });

  it("lists the test_player.glb placeholder track and 25 joints", []() {
    expect(runInfo(FIXTURES_DIR "/test_player.glb", NULL)).toBe((int)ERR_NONE);
    std::string text = capturedText();
    expect(text).toMatch("\"mixamo.com\"  0.067 s  2 keys @ 30 fps  75 channels (T 25, R 25, S 25)  STEP\n");
    expect(text).toMatch("armature (skin 0, root node 26 \"Armature\"):\n  mixamorig:Hips\n");
    expect(text).toMatch("              mixamorig:RightHand\n");
    expect(text).Not->toMatch("HUmar body");
  });

  it("appends rest transforms with --rest", []() {
    expect(runInfo(FIXTURES_DIR "/test_player.glb", "--rest")).toBe((int)ERR_NONE);
    expect(capturedText()).toMatch("  mixamorig:Hips  T=(-5.5297, -10.8019, -412.6846) "
      "R=(-0.7071, 0.0000, 0.0000, 0.7071) S=(1.0000, 1.0000, 1.0000)\n");
  });

  it("lists the 43 library tracks of UAL1_Standard_RM.glb", []() {
    expect(runInfo(FIXTURES_DIR "/UAL1_Standard_RM.glb", NULL)).toBe((int)ERR_NONE);
    std::string text = capturedText();
    expect(text).toMatch("animations: 43\n  [0] \"A_TPose\"  2.500 s  76 keys @ 30 fps  "
      "195 channels (T 65, R 65, S 65)  LINEAR\n");
    expect(text).toMatch("  [42] \"Walk_Loop\"");
    expect(text).toMatch("armature (skin 0, root node 66 \"Armature\"):\n  root\n    pelvis\n");
  });

  it("fails cleanly on a missing file", []() {
    expect(runInfo(FIXTURES_DIR "/missing.glb", NULL)).toBe((int)ERR_OPEN_INPUT);
  });

  it("rejects an unknown flag", []() {
    expect(runInfo(FIXTURES_DIR "/sword_run.glb", "--bogus")).toBe((int)ERR_BAD_ARGS);
  });
});
