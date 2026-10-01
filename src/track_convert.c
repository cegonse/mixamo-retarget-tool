#include <anim_track.h>
#include <track_convert.h>

static ErrorCode sampleClip(ConvertSession *session, const AnimTrack *track, AnimationClip *clip,
    Pose *source_pose, Pose *destination_pose) {
  size_t frame;
  ErrorCode error = ERR_NONE;
  Retarget_BeginTrack(session->retarget);
  for (frame = 0; frame < AnimationClip_FrameCount(clip) && error == ERR_NONE; frame++) {
    float time = AnimTrack_Start(track) + (float)frame / AnimationClip_Fps(clip);
    AnimTrack_EvaluatePose(track, time, source_pose);
    error = Retarget_Frame(session->retarget, source_pose, destination_pose);
    AnimationClip_SetFrame(clip, frame, destination_pose);
  }
  return error;
}

AnimationClip *TrackConvert_Clip(ConvertSession *session, size_t animation, int has_fps,
    float fps, ErrorCode *error) {
  AnimTrack *track = AnimTrack_FromDoc(session->source_doc, animation, session->source, error);
  Pose *source_pose = Pose_Create(Skeleton_JointCount(session->source));
  Pose *destination_pose = Pose_Create(Skeleton_JointCount(session->destination));
  AnimationClip *clip = NULL;
  if (track != NULL && source_pose != NULL && destination_pose != NULL) {
    float clip_fps = has_fps ? fps : AnimTrack_Fps(track);
    clip = AnimationClip_Create(AnimTrack_Name(track), Skeleton_JointCount(session->destination),
      AnimTrack_FrameCount(track, clip_fps), clip_fps);
    *error = clip != NULL ? sampleClip(session, track, clip, source_pose, destination_pose)
      : ERR_INTERNAL;
  } else if (*error == ERR_NONE) {
    *error = ERR_INTERNAL;
  }
  Pose_Destroy(source_pose);
  Pose_Destroy(destination_pose);
  AnimTrack_Destroy(track);
  if (*error != ERR_NONE) {
    AnimationClip_Destroy(clip);
    return NULL;
  }
  return clip;
}
