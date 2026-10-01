#pragma once
#include <animation_clip.h>
#include <convert_session.h>

AnimationClip *TrackConvert_Clip(ConvertSession *session, size_t animation, int has_fps,
  float fps, ErrorCode *error);
