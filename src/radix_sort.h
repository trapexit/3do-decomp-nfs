#pragma once

#include "geometry.h"

#include "types.h"

typedef struct PolygonDepthRecord
{
  int32 reserved[2];
  int32 vertex_indices[4];
} PolygonDepthRecord;

extern int32 g_POLYGON_DEPTH_SORT_THRESHOLD;

void
sort_words_by_low16_descending(int32 count_, uint32 *words_, uint32 *scratch_);
void
build_sorted_polygon_depth_keys(int32                     polygon_count_,
                                const PolygonDepthRecord *polygons_,
                                const Vector3            *vertices_,
                                uint32                   *keys_,
                                uint32                   *scratch_);
