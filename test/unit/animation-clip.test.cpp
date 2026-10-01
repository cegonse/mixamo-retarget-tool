#include <cest>
#include <string>
extern "C" {
#include <animation_clip.h>
}

static AnimationClip *clip = nullptr;
static Pose *pose = nullptr;

describe("AnimationClip", []() {
  beforeEach([]() {
    clip = AnimationClip_Create("Walk", 2, 3, 30.0f);
    pose = Pose_Create(2);
  });

  afterEach([]() {
    AnimationClip_Destroy(clip);
    Pose_Destroy(pose);
    clip = nullptr;
    pose = nullptr;
  });

  it("reports its shape and timing from zero", []() {
    expect(std::string(AnimationClip_Name(clip))).toBe("Walk");
    expect(AnimationClip_JointCount(clip)).toBe((size_t)2);
    expect(AnimationClip_FrameCount(clip)).toBe((size_t)3);
    expect(AnimationClip_FrameTime(clip, 0)).toBe(0.0f);
    expect(AnimationClip_FrameTime(clip, 1)).toBe(1.0f / 30.0f, 1e-7f);
    expect(AnimationClip_Duration(clip)).toBe(2.0f / 30.0f, 1e-7f);
  });

  it("stores per-joint keyframes and the first-frame scale", []() {
    for (size_t frame = 0; frame < 3; frame++) {
      for (size_t joint = 0; joint < 2; joint++) {
        Transform *local = Pose_Local(pose, joint);
        Transform_Identity(local);
        local->translation[0] = (float)(10 * joint + frame);
        local->scale[1] = 2.0f + (float)frame;
      }
      AnimationClip_SetFrame(clip, frame, pose);
    }
    expect(AnimationClip_Translations(clip, 1)[3 * 2]).toBe(12.0f);
    expect(AnimationClip_Rotations(clip, 0)[4 * 1 + 3]).toBe(1.0f);
    expect(AnimationClip_Scale(clip, 1)[1]).toBe(2.0f);
  });

  it("destroys NULL harmlessly", []() {
    AnimationClip_Destroy(NULL);
  });
});
