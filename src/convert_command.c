#include <anim_track.h>
#include <animation_clip.h>
#include <convert_command.h>
#include <convert_report.h>
#include <convert_session.h>
#include <file_io.h>
#include <glb_writer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <track_selection.h>

static int isSafeCharacter(char character) {
  return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
    || (character >= '0' && character <= '9') || character == '.' || character == '_'
    || character == '-';
}

char *ConvertCommand_OutputPath(const char *out_dir, const char *track_name) {
  size_t directory_length = strlen(out_dir), name_length = strlen(track_name), index;
  char *path = malloc(directory_length + name_length + 6);
  char *name;
  if (path == NULL) {
    return NULL;
  }
  memcpy(path, out_dir, directory_length);
  path[directory_length] = '/';
  name = path + directory_length + 1;
  for (index = 0; index < name_length; index++) {
    name[index] = isSafeCharacter(track_name[index]) ? track_name[index] : '_';
  }
  strcpy(name + name_length, ".glb");
  return path;
}

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

static AnimationClip *retargetTrack(ConvertSession *session, const Args *args, size_t animation,
    ErrorCode *error) {
  AnimTrack *track = AnimTrack_FromDoc(session->source_doc, animation, session->source, error);
  Pose *source_pose = Pose_Create(Skeleton_JointCount(session->source));
  Pose *destination_pose = Pose_Create(Skeleton_JointCount(session->destination));
  AnimationClip *clip = NULL;
  if (track != NULL && source_pose != NULL && destination_pose != NULL) {
    float fps = args->has_fps ? args->fps : AnimTrack_Fps(track);
    clip = AnimationClip_Create(AnimTrack_Name(track), Skeleton_JointCount(session->destination),
      AnimTrack_FrameCount(track, fps), fps);
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

static ErrorCode convertTrack(FILE *output, ConvertSession *session, const Args *args,
    size_t animation) {
  ErrorCode error;
  size_t bytes = 0;
  AnimationClip *clip = retargetTrack(session, args, animation, &error);
  char *path;
  if (clip == NULL) {
    return error;
  }
  path = args->out_file != NULL ? NULL : ConvertCommand_OutputPath(args->out_dir, AnimationClip_Name(clip));
  ConvertReport_Track(output, clip);
  ConvertReport_Alignment(output, Retarget_Alignment(session->retarget));
  ConvertReport_Joints(output, session, args->verbose);
  error = GlbWriter_Write(path != NULL ? path : args->out_file, session->destination_doc,
    session->destination, clip, &bytes);
  if (error == ERR_NONE) {
    ConvertReport_Wrote(output, path != NULL ? path : args->out_file, bytes);
  }
  free(path);
  AnimationClip_Destroy(clip);
  return error;
}

static ErrorCode ensureParentDirectory(const char *file_path) {
  const char *slash = strrchr(file_path, '/');
  size_t length = slash != NULL ? (size_t)(slash - file_path) : 0;
  char *directory;
  ErrorCode error;
  if (length == 0) {
    return ERR_NONE;
  }
  directory = malloc(length + 1);
  if (directory == NULL) {
    return ERR_INTERNAL;
  }
  memcpy(directory, file_path, length);
  directory[length] = '\0';
  error = FileIo_EnsureDirectory(directory);
  free(directory);
  return error;
}

static ErrorCode ensureOutputDirectory(const Args *args) {
  const char *target = args->out_dir != NULL ? args->out_dir : args->out_file;
  ErrorCode error = args->out_dir != NULL ? FileIo_EnsureDirectory(args->out_dir)
    : ensureParentDirectory(args->out_file);
  if (error != ERR_NONE) {
    fprintf(stderr, "error: cannot create the output directory for %s\n", target);
  }
  return error;
}

static ErrorCode convertSelected(FILE *output, ConvertSession *session, const Args *args) {
  size_t count = 0, index, animation_count = GltfDoc_AnimationCount(session->source_doc);
  size_t *indices = malloc((animation_count > 0 ? animation_count : 1) * sizeof *indices);
  ErrorCode error = indices != NULL ? ERR_NONE : ERR_INTERNAL;
  if (error == ERR_NONE) {
    error = TrackSelection_Resolve(session->source_doc, args, indices, &count);
  }
  if (error == ERR_NONE && args->out_file != NULL && count != 1) {
    fprintf(stderr, "error: --out needs exactly one track (%zu selected); use --out-dir\n", count);
    error = ERR_BAD_ARGS;
  }
  if (error == ERR_NONE) {
    error = ensureOutputDirectory(args);
  }
  for (index = 0; index < count && error == ERR_NONE; index++) {
    error = convertTrack(output, session, args, indices[index]);
  }
  free(indices);
  return error;
}

ErrorCode ConvertCommand_Run(FILE *output, const Args *args) {
  ConvertSession session;
  ErrorCode error = ConvertSession_Open(&session, args);
  if (error != ERR_NONE) {
    return error;
  }
  error = convertSelected(output, &session, args);
  ConvertSession_Close(&session);
  return error;
}
