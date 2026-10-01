#pragma once
#include <stddef.h>

void WebViewer_Start(void);
void WebViewer_Resize(int width, int height);
int WebViewer_LoadModel(const char *path);
void WebViewer_SetShowBones(int show);
void WebViewer_ResetCamera(void);
int WebViewer_SetAnimation(size_t frame_count, size_t joint_count, float fps, const float *poses);
void WebViewer_ClearAnimation(void);
void WebViewer_SetPlaying(int playing);
int WebViewer_IsPlaying(void);
void WebViewer_SetFrame(int frame);
int WebViewer_Frame(void);
int WebViewer_FrameCount(void);
