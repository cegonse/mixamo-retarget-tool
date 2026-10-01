#include <convert_report.h>
#include <transform.h>

static const float residual_warning_ratio = 0.2f;

static void printThousands(FILE *output, size_t value) {
  if (value >= 1000) {
    printThousands(output, value / 1000);
    fprintf(output, ",%03zu", value % 1000);
    return;
  }
  fprintf(output, "%zu", value);
}

void ConvertReport_Alignment(FILE *output, const FrameAlignment *alignment) {
  vec3 degrees;
  Transform_EulerDegrees((float *)alignment->rotation, degrees);
  fprintf(output, "  frame: rotate (%.1f\xC2\xB0, %.1f\xC2\xB0, %.1f\xC2\xB0)  scale %.2f  rms %.1f\n",
    degrees[0], degrees[1], degrees[2], alignment->scale, alignment->rms_residual);
  if (alignment->degenerate) {
    fprintf(stderr, "warning: fewer than 3 non-collinear mapped joints; frame alignment is identity\n");
  } else if (alignment->rms_residual > residual_warning_ratio * alignment->destination_spread) {
    fprintf(stderr, "warning: frame alignment residual %.1f exceeds 20%% of the joint spread %.1f; "
      "check the bone map\n", alignment->rms_residual, alignment->destination_spread);
  }
}

static void listUnmapped(FILE *output, const char *label, const Skeleton *skeleton,
    const BoneMap *map, int source_side) {
  size_t joint;
  fprintf(output, "  %s unmapped:", label);
  for (joint = 0; joint < Skeleton_JointCount(skeleton); joint++) {
    size_t pair = source_side ? BoneMap_PairForSource(map, joint)
      : BoneMap_PairForDestination(map, joint);
    if (pair == BONE_MAP_NO_PAIR) {
      fprintf(output, " %s", Skeleton_JointName(skeleton, joint));
    }
  }
  fputc('\n', output);
}

void ConvertReport_Joints(FILE *output, const ConvertSession *session, int verbose) {
  size_t mapped = BoneMap_PairCount(session->map);
  fprintf(output, "  joints: %zu mapped, %zu source unmapped, %zu destination unmapped\n", mapped,
    Skeleton_JointCount(session->source) - mapped,
    Skeleton_JointCount(session->destination) - mapped);
  if (verbose) {
    listUnmapped(output, "source", session->source, session->map, 1);
    listUnmapped(output, "destination", session->destination, session->map, 0);
  }
}

void ConvertReport_Track(FILE *output, const AnimationClip *clip) {
  fprintf(output, "track \"%s\": %zu frames @ %g fps (%.3f s)\n", AnimationClip_Name(clip),
    AnimationClip_FrameCount(clip), AnimationClip_Fps(clip), AnimationClip_Duration(clip));
}

void ConvertReport_Wrote(FILE *output, const char *path, size_t bytes) {
  fprintf(output, "  wrote %s (", path);
  printThousands(output, bytes);
  fputs(" bytes)\n", output);
}
