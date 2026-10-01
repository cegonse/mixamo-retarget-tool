#include <cest>
#include <string>
#include <vector>
extern "C" {
#include <args.h>
}

static Args parsed;

static int parse(std::vector<std::string> arguments) {
  static std::vector<std::string> storage;
  std::vector<char *> argv;
  storage = arguments;
  storage.insert(storage.begin(), "anim-retarget");
  for (std::string &argument : storage) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  return (int)Args_Parse((int)storage.size(), argv.data(), &parsed);
}

static std::vector<std::string> convertWith(std::vector<std::string> extra) {
  std::vector<std::string> arguments = {"convert", "src.glb", "dst.glb", "--anim", "Walk",
    "--map", "a=b", "--out-dir", "out"};
  arguments.insert(arguments.end(), extra.begin(), extra.end());
  return arguments;
}

describe("Args", []() {
  it("rejects a missing or unknown command", []() {
    expect(parse({})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"render"})).toBe((int)ERR_BAD_ARGS);
  });

  it("parses help", []() {
    expect(parse({"--help"})).toBe((int)ERR_NONE);
    expect((int)parsed.command).toBe((int)COMMAND_HELP);
    expect(parse({"-h"})).toBe((int)ERR_NONE);
  });

  it("parses info with and without --rest", []() {
    expect(parse({"info", "a.glb"})).toBe((int)ERR_NONE);
    expect((int)parsed.command).toBe((int)COMMAND_INFO);
    expect(std::string(parsed.source_path)).toBe("a.glb");
    expect(parsed.show_rest).toBe(0);
    expect(parse({"info", "--rest", "a.glb"})).toBe((int)ERR_NONE);
    expect(parsed.show_rest).toBe(1);
  });

  it("rejects bad info arguments", []() {
    expect(parse({"info"})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"info", "a.glb", "b.glb"})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"info", "a.glb", "--rest", "--rest"})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"info", "a.glb", "--bogus"})).toBe((int)ERR_BAD_ARGS);
  });

  it("parses a minimal convert with defaults", []() {
    expect(parse(convertWith({}))).toBe((int)ERR_NONE);
    expect((int)parsed.command).toBe((int)COMMAND_CONVERT);
    expect(std::string(parsed.source_path)).toBe("src.glb");
    expect(std::string(parsed.destination_path)).toBe("dst.glb");
    expect(std::string(parsed.animations)).toBe("Walk");
    expect(std::string(parsed.out_dir)).toBe("out");
    expect(parsed.has_fps).toBe(0);
    expect(parsed.retarget.frame_align).toBe(1);
    expect(parsed.retarget.rest_align).toBe(1);
    expect(parsed.retarget.in_place).toBe(0);
    expect(parsed.retarget.source_up[1]).toBe(1.0f);
    expect(parsed.verbose).toBe(0);
  });

  it("parses every convert flag", []() {
    expect(parse({"convert", "--all-anims", "s.glb", "d.glb", "--map-file", "m.map", "--map",
      "x=y", "--out", "o.glb", "--fps", "60", "--in-place", "--src-up", "-Z", "--no-frame-align",
      "--frame-rotate", "-90,0,45.5", "--frame-scale", "450", "--no-rest-align", "--verbose"}))
      .toBe((int)ERR_NONE);
    expect(parsed.all_animations).toBe(1);
    expect(parsed.animations == nullptr).toBeTruthy();
    expect(parsed.map_source_count).toBe((size_t)2);
    expect(parsed.map_sources[0].is_file).toBe(1);
    expect(std::string(parsed.map_sources[1].text)).toBe("x=y");
    expect(std::string(parsed.out_file)).toBe("o.glb");
    expect(parsed.fps).toBe(60.0f);
    expect(parsed.retarget.in_place).toBe(1);
    expect(parsed.retarget.source_up[2]).toBe(-1.0f);
    expect(parsed.retarget.source_up[1]).toBe(0.0f);
    expect(parsed.retarget.frame_align).toBe(0);
    expect(parsed.retarget.has_frame_rotation).toBe(1);
    expect(parsed.retarget.frame_rotation_degrees[2]).toBe(45.5f);
    expect(parsed.retarget.frame_scale).toBe(450.0f);
    expect(parsed.retarget.rest_align).toBe(0);
    expect(parsed.verbose).toBe(1);
  });

  it("keeps repeated --map and --map-file in command-line order", []() {
    expect(parse(convertWith({"--map-file", "f.map", "--map", "c=d"}))).toBe((int)ERR_NONE);
    expect(parsed.map_source_count).toBe((size_t)3);
    expect(std::string(parsed.map_sources[0].text)).toBe("a=b");
    expect(std::string(parsed.map_sources[1].text)).toBe("f.map");
    expect(std::string(parsed.map_sources[2].text)).toBe("c=d");
  });

  it("requires the convert positionals and choices", []() {
    expect(parse({"convert", "s.glb", "--anim", "W", "--map", "a=b", "--out-dir", "o"})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"convert", "s.glb", "d.glb", "--map", "a=b", "--out-dir", "o"})).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--all-anims"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse({"convert", "s.glb", "d.glb", "--anim", "W", "--out-dir", "o"})).toBe((int)ERR_BAD_ARGS);
    expect(parse({"convert", "s.glb", "d.glb", "--anim", "W", "--map", "a=b"})).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--out", "x.glb"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"extra.glb"}))).toBe((int)ERR_BAD_ARGS);
  });

  it("rejects duplicate single-use flags", []() {
    expect(parse(convertWith({"--anim", "Run"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--fps", "30", "--fps", "60"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--in-place", "--in-place"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--out-dir", "again"}))).toBe((int)ERR_BAD_ARGS);
  });

  it("rejects invalid flag values", []() {
    expect(parse(convertWith({"--fps", "0"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--fps", "-30"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--fps", "thirty"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--fps", "nan"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--src-up", "W"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--frame-rotate", "1,2"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--frame-rotate", "1,2,3,4"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--frame-rotate", "a,b,c"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--frame-scale", "0"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--anim", ""}))).toBe((int)ERR_BAD_ARGS);
  });

  it("rejects unknown flags and missing values", []() {
    expect(parse(convertWith({"--bogus"}))).toBe((int)ERR_BAD_ARGS);
    expect(parse(convertWith({"--fps"}))).toBe((int)ERR_BAD_ARGS);
  });
});
