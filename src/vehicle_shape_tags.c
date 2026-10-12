#include "vehicle_shape_tags.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

const char G_VEHICLE_SHAPE_TAGS[VEHICLE_SHAPE_TAG_COUNT][VEHICLE_SHAPE_TAG_SIZE] = {
  "inside", "surface", "outside", "lt_frnt", "rt_frnt", "lt_rear", "rt_rear", "whl0", "whl1",
  "whl2",   "bitf",    "bitr",    "bits",    "mapv",    "lfl0",    "lfr0",    "lrl0", "lrr0",
  "lfc0",   "lfc0",    "lrc0",    "lrc0",    "lfl1",    "lfl1",    "lfr1",    "lfr1", "lrl1",
  "lrl2",   "lrr1",    "lrr2",    "bkll",    "bklr",    "bkl1",    "XXXX"
};
const char *g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_SHAPE_TAG_COUNT] = {
  G_VEHICLE_SHAPE_TAGS[0],  G_VEHICLE_SHAPE_TAGS[1],  G_VEHICLE_SHAPE_TAGS[2],
  G_VEHICLE_SHAPE_TAGS[3],  G_VEHICLE_SHAPE_TAGS[4],  G_VEHICLE_SHAPE_TAGS[5],
  G_VEHICLE_SHAPE_TAGS[6],  G_VEHICLE_SHAPE_TAGS[7],  G_VEHICLE_SHAPE_TAGS[8],
  G_VEHICLE_SHAPE_TAGS[9],  G_VEHICLE_SHAPE_TAGS[10], G_VEHICLE_SHAPE_TAGS[11],
  G_VEHICLE_SHAPE_TAGS[12], G_VEHICLE_SHAPE_TAGS[13], G_VEHICLE_SHAPE_TAGS[14],
  G_VEHICLE_SHAPE_TAGS[15], G_VEHICLE_SHAPE_TAGS[16], G_VEHICLE_SHAPE_TAGS[17],
  G_VEHICLE_SHAPE_TAGS[18], G_VEHICLE_SHAPE_TAGS[19], G_VEHICLE_SHAPE_TAGS[20],
  G_VEHICLE_SHAPE_TAGS[21], G_VEHICLE_SHAPE_TAGS[22], G_VEHICLE_SHAPE_TAGS[23],
  G_VEHICLE_SHAPE_TAGS[24], G_VEHICLE_SHAPE_TAGS[25], G_VEHICLE_SHAPE_TAGS[26],
  G_VEHICLE_SHAPE_TAGS[27], G_VEHICLE_SHAPE_TAGS[28], G_VEHICLE_SHAPE_TAGS[29],
  G_VEHICLE_SHAPE_TAGS[30], G_VEHICLE_SHAPE_TAGS[31], G_VEHICLE_SHAPE_TAGS[32],
  G_VEHICLE_SHAPE_TAGS[33]
};

COMPILE_TIME_SIZE_ASSERT(vehicle_shape_tags_size, sizeof(G_VEHICLE_SHAPE_TAGS) == 272);
COMPILE_TIME_SIZE_ASSERT(vehicle_shape_tag_pointer_size,
                         sizeof(g_VEHICLE_SHAPE_TAG_POINTERS) == 136);
