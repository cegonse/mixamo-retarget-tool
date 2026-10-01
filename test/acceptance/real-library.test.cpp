#include <cest>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
extern "C" {
#include <anim_track.h>
#include <app_main.h>
}

static const std::string library = FIXTURES_DIR "/UAL1_Standard_RM.glb";
static const std::string player = FIXTURES_DIR "/test_player.glb";
static const std::string ual_map = DOCS_DIR "/mappings/ual-to-mixamo.map";
static const std::string out_dir = std::string(TEST_OUTPUT_DIR) + "/real-library";
static const float model_top_z = -646.0f;
static const float model_floor_z = 4.0f;
static FILE *captured = nullptr;

struct Output {
  GltfDoc *doc = nullptr;
  Skeleton *skeleton = nullptr;
  AnimTrack *track = nullptr;
  Pose *pose = nullptr;

  explicit Output(const std::string &path) {
    ErrorCode error = ERR_INTERNAL;
    doc = GltfDoc_Load(path.c_str(), &error);
    expect((int)error).toBe((int)ERR_NONE);
    skeleton = Skeleton_FromDoc(doc, 0, &error);
    track = AnimTrack_FromDoc(doc, 0, skeleton, &error);
    pose = Pose_Create(Skeleton_JointCount(skeleton));
  }

  ~Output() {
    Pose_Destroy(pose);
    AnimTrack_Destroy(track);
    Skeleton_Destroy(skeleton);
    GltfDoc_Destroy(doc);
  }

  Output(const Output &) = delete;
  Output &operator=(const Output &) = delete;
};

static int run(std::vector<std::string> arguments) {
  std::vector<char *> argv;
  arguments.insert(arguments.begin(), "anim-retarget");
  for (std::string &argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  return App_Run((int)arguments.size(), argv.data());
}

static size_t frameCount(const Output &output) {
  return AnimTrack_FrameCount(output.track, AnimTrack_Fps(output.track));
}

static void evaluate(Output &output, size_t frame) {
  AnimTrack_EvaluatePose(output.track, (float)frame / AnimTrack_Fps(output.track), output.pose);
  Pose_ComputeGlobals(output.pose, output.skeleton);
}

static bool allFinite(const Output &output) {
  for (size_t channel = 0; channel < GltfDoc_ChannelCount(output.doc, 0); channel++) {
    GltfChannel entry = GltfDoc_Channel(output.doc, 0, channel);
    std::vector<float> values(GltfDoc_SamplerOutputFloatCount(output.doc, 0, entry.sampler));
    GltfDoc_SamplerOutput(output.doc, 0, entry.sampler, values.data());
    for (float value : values) {
      if (!std::isfinite(value)) {
        return false;
      }
    }
  }
  return true;
}

static bool hipsInside(Output &output) {
  for (size_t frame = 0; frame < frameCount(output); frame++) {
    evaluate(output, frame);
    float hips_z = Pose_Global(output.pose, 0)->translation[2];
    if (hips_z < model_top_z || hips_z > model_floor_z) {
      return false;
    }
  }
  return true;
}

static float lowestFootZ(Output &output) {
  float lowest = -INFINITY;
  for (const char *name : {"mixamorig:LeftFoot", "mixamorig:RightFoot", "mixamorig:LeftToe_End",
      "mixamorig:RightToe_End"}) {
    lowest = fmaxf(lowest, Pose_Global(output.pose, Skeleton_FindJoint(output.skeleton, name))->translation[2]);
  }
  return lowest;
}

describe("the real library on test_player", []() {
  beforeAll([]() {
    captured = tmpfile();
    App_SetOutputStream(captured);
    run({"convert", library, player, "--all-anims", "--map-file", ual_map, "--out-dir", out_dir,
      "--in-place", "--no-rest-align"});
    App_SetOutputStream(NULL);
    fclose(captured);
  });

  afterAll([]() {
    std::filesystem::remove_all(out_dir);
  });

  it("produces 43 finite files whose hips stay inside the model's height", []() {
    ErrorCode error;
    GltfDoc *source = GltfDoc_Load(library.c_str(), &error);
    std::vector<std::string> names;
    for (size_t animation = 0; animation < GltfDoc_AnimationCount(source); animation++) {
      names.push_back(GltfDoc_AnimationName(source, animation));
    }
    GltfDoc_Destroy(source);
    std::string non_finite, outside;
    for (const std::string &name : names) {
      Output output(out_dir + "/" + name + ".glb");
      if (!allFinite(output)) {
        non_finite += name + " ";
      }
      if (name != "Swim_Idle_Loop" && !hipsInside(output)) {
        outside += name + " ";
      }
    }
    expect(names.size()).toBe((size_t)43);
    expect(non_finite).toBe("");
    expect(outside).toBe("");
  });

  it("follows the source below the floor while treading water", []() {
    Output output(out_dir + "/Swim_Idle_Loop.glb");
    evaluate(output, 0);
    expect(Pose_Global(output.pose, 0)->translation[2]).toBeGreaterThan(model_floor_z);
  });

  it("turns A_TPose into the destination rest pose", []() {
    Output output(out_dir + "/A_TPose.glb");
    evaluate(output, 0);
    for (size_t joint = 0; joint < Skeleton_JointCount(output.skeleton); joint++) {
      float dot = fabsf(glm_quat_dot(Pose_Local(output.pose, joint)->rotation,
        (float *)Skeleton_RestLocal(output.skeleton, joint)->rotation));
      expect(2.0f * acosf(fminf(1.0f, dot)) * 180.0f / (float)M_PI).toBeLessThan(3.0f);
    }
  });

  it("keeps the idle feet on the floor", []() {
    Output output(out_dir + "/Idle_Loop.glb");
    for (size_t frame = 0; frame < frameCount(output); frame++) {
      evaluate(output, frame);
      expect(lowestFootZ(output)).toBeInRange(-5.0f, 3.0f);
    }
  });

  it("keeps the head above the hips along -Z", []() {
    Output output(out_dir + "/Walk_Loop.glb");
    size_t head = Skeleton_FindJoint(output.skeleton, "mixamorig:Head");
    for (size_t frame = 0; frame < frameCount(output); frame++) {
      evaluate(output, frame);
      expect(Pose_Global(output.pose, head)->translation[2] - Pose_Global(output.pose, 0)->translation[2])
        .toBeLessThan(-150.0f);
    }
  });

  it("loops the in-place jog without a seam", []() {
    Output output(out_dir + "/Jog_Fwd_Loop.glb");
    evaluate(output, 0);
    Transform first = *Pose_Local(output.pose, 0);
    evaluate(output, frameCount(output) - 1);
    for (int axis = 0; axis < 3; axis++) {
      expect(Pose_Local(output.pose, 0)->translation[axis]).toBe(first.translation[axis], 0.5f);
    }
  });
});
