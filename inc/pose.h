#pragma once
#include <skeleton.h>

typedef struct Pose Pose;

Pose *Pose_Create(size_t joint_count);
void Pose_Destroy(Pose *self);
size_t Pose_JointCount(const Pose *self);
Transform *Pose_Local(Pose *self, size_t joint);
Transform *Pose_Global(Pose *self, size_t joint);
void Pose_SetRest(Pose *self, const Skeleton *skeleton);
void Pose_ComputeGlobals(Pose *self, const Skeleton *skeleton);
