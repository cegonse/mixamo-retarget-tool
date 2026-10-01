#pragma once
#include <animation_clip.h>
#include <convert_session.h>
#include <frame_align.h>
#include <stdio.h>

void ConvertReport_Alignment(FILE *output, const FrameAlignment *alignment);
void ConvertReport_Joints(FILE *output, const ConvertSession *session, int verbose);
void ConvertReport_Track(FILE *output, const AnimationClip *clip);
void ConvertReport_Wrote(FILE *output, const char *path, size_t bytes);
