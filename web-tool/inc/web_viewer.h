#pragma once

void WebViewer_Start(void);
void WebViewer_Resize(int width, int height);
int WebViewer_LoadModel(const char *path);
void WebViewer_SetShowBones(int show);
void WebViewer_ResetCamera(void);
