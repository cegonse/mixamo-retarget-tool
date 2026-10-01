#include <gltf_doc.h>
#include <info_command.h>
#include <track_timing.h>

typedef struct ChannelSummary {
  size_t translation;
  size_t rotation;
  size_t scale;
  int seen_interpolation[3];
  Interpolation interpolation_order[3];
  size_t interpolation_count;
} ChannelSummary;

static const char *const interpolation_names[] = {"LINEAR", "STEP", "CUBICSPLINE"};

static void countChannel(GltfDoc *doc, size_t animation, size_t channel,
    ChannelSummary *summary) {
  GltfChannel entry = GltfDoc_Channel(doc, animation, channel);
  Interpolation interpolation = GltfDoc_SamplerInterpolation(doc, animation, entry.sampler);
  summary->translation += entry.path == ANIMATION_PATH_TRANSLATION;
  summary->rotation += entry.path == ANIMATION_PATH_ROTATION;
  summary->scale += entry.path == ANIMATION_PATH_SCALE;
  if (!summary->seen_interpolation[interpolation]) {
    summary->seen_interpolation[interpolation] = 1;
    summary->interpolation_order[summary->interpolation_count++] = interpolation;
  }
}

static void printInterpolations(FILE *output, const ChannelSummary *summary) {
  size_t index;
  for (index = 0; index < summary->interpolation_count; index++) {
    fprintf(output, "%s%s", index > 0 ? "," : "",
      interpolation_names[summary->interpolation_order[index]]);
  }
  fputc('\n', output);
}

static ErrorCode printAnimation(FILE *output, GltfDoc *doc, size_t animation) {
  ChannelSummary summary = {0};
  TrackTiming timing;
  size_t channel, channel_count = GltfDoc_ChannelCount(doc, animation);
  ErrorCode error = TrackTiming_FromDoc(doc, animation, &timing);
  if (error != ERR_NONE) {
    return error;
  }
  for (channel = 0; channel < channel_count; channel++) {
    countChannel(doc, animation, channel, &summary);
  }
  fprintf(output, "  [%zu] \"%s\"  %.3f s  %zu keys @ %.0f fps  %zu channels (T %zu, R %zu, S %zu)  ",
    animation, GltfDoc_AnimationName(doc, animation), timing.end, timing.max_keys, timing.fps,
    channel_count, summary.translation, summary.rotation, summary.scale);
  printInterpolations(output, &summary);
  return ERR_NONE;
}

static ErrorCode printAnimations(FILE *output, GltfDoc *doc) {
  size_t animation;
  ErrorCode error = ERR_NONE;
  fprintf(output, "animations: %zu\n", GltfDoc_AnimationCount(doc));
  for (animation = 0; animation < GltfDoc_AnimationCount(doc) && error == ERR_NONE; animation++) {
    error = printAnimation(output, doc, animation);
  }
  return error;
}

static void printSkins(FILE *output, GltfDoc *doc) {
  size_t skin;
  fprintf(output, "skins: %zu\n", GltfDoc_SkinCount(doc));
  for (skin = 0; skin < GltfDoc_SkinCount(doc); skin++) {
    fprintf(output, "  [%zu] \"%s\"  %zu joints  inverseBindMatrices: %s\n", skin,
      GltfDoc_SkinName(doc, skin), GltfDoc_SkinJointCount(doc, skin),
      GltfDoc_SkinHasInverseBindMatrices(doc, skin) ? "yes" : "no");
  }
}


ErrorCode InfoCommand_Run(FILE *output, const char *path, int show_rest) {
  ErrorCode error;
  GltfDoc *doc = GltfDoc_Load(path, &error);
  if (doc == NULL) {
    return error;
  }
  fprintf(output, "file: %s\n", path);
  fprintf(output, "asset: glTF %s, generator \"%s\"\n", GltfDoc_Version(doc),
    GltfDoc_Generator(doc));
  error = printAnimations(output, doc);
  if (error == ERR_NONE) {
    printSkins(output, doc);
    error = InfoCommand_PrintArmature(output, doc, show_rest);
  }
  GltfDoc_Destroy(doc);
  return error;
}
