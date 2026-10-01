#include <frame_align.h>
#include <stdlib.h>

ErrorCode FrameAlign_FromMap(const BoneMap *map, const Skeleton *source,
    const Skeleton *destination, FrameAlignment *dest) {
  size_t pair, count = BoneMap_PairCount(map);
  vec3 *source_points = malloc((count > 0 ? count : 1) * sizeof *source_points);
  vec3 *destination_points = malloc((count > 0 ? count : 1) * sizeof *destination_points);
  if (source_points == NULL || destination_points == NULL) {
    free(source_points);
    free(destination_points);
    return ERR_INTERNAL;
  }
  for (pair = 0; pair < count; pair++) {
    glm_vec3_copy((float *)Skeleton_RestGlobal(source, BoneMap_SourceJoint(map, pair))->translation,
      source_points[pair]);
    glm_vec3_copy(
      (float *)Skeleton_RestGlobal(destination, BoneMap_DestinationJoint(map, pair))->translation,
      destination_points[pair]);
  }
  FrameAlign_Solve(source_points, destination_points, count, dest);
  free(source_points);
  free(destination_points);
  return ERR_NONE;
}
