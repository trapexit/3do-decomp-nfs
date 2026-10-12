#include "radix_sort.h"

#define RADIX_BUCKET_COUNT 256
#define RADIX_BYTE_MASK 0xFF
#define RADIX_HIGH_BYTE_SHIFT 8
#define RADIX_HIGH_COUNT_SHIFT 16
#define RADIX_HIGH_COUNT_UNIT 0x10000
#define RADIX_LOW_COUNT_MASK 0xFFFF
#define RADIX_UNROLLED_WORDS 4
#define POLYGON_VERTEX_COUNT 4
#define POLYGON_KEY_INDEX_SHIFT 16
#define POLYGON_DEPTH_THRESHOLD_DEFAULT 150

typedef char PolygonDepthRecordSize[(sizeof(PolygonDepthRecord) == 24) ? 1 : -1];

int32 g_POLYGON_DEPTH_SORT_THRESHOLD = POLYGON_DEPTH_THRESHOLD_DEFAULT;

void
build_sorted_polygon_depth_keys(int32                     polygon_count_,
                                const PolygonDepthRecord *polygons_,
                                const Vector3            *vertices_,
                                uint32                   *keys_,
                                uint32                   *scratch_)
{
  int32 depth;
  int32 minimum_depth;
  int32 polygon_index;
  int32 vertex_index;
  int32 vertex_slot;

  for(polygon_index = 0; polygon_index < polygon_count_; polygon_index++)
    {
      vertex_index  = polygons_[polygon_index].vertex_indices[0];
      minimum_depth = vertices_[vertex_index].z;
      for(vertex_slot = 1; vertex_slot < POLYGON_VERTEX_COUNT; vertex_slot++)
        {
          vertex_index = polygons_[polygon_index].vertex_indices[vertex_slot];
          depth        = vertices_[vertex_index].z;
          if(depth < minimum_depth)
            minimum_depth = depth;
        }

      if(minimum_depth < g_POLYGON_DEPTH_SORT_THRESHOLD)
        minimum_depth = 0;
      keys_[polygon_index] =
        ((uint32) polygon_index << POLYGON_KEY_INDEX_SHIFT) | (uint32) minimum_depth;
    }

  sort_words_by_low16_descending(polygon_count_, keys_, scratch_);
}

void
sort_words_by_low16_descending(int32 count_, uint32 *words_, uint32 *scratch_)
{
  uint32  packed_counts[RADIX_BUCKET_COUNT];
  uint32 *bucket_cursors[RADIX_BUCKET_COUNT];
  uint32 *input_cursor;
  uint32 *output_cursor;
  uint32  word;
  int32   remaining;
  int32   bucket;
  int32   index;

  for(bucket = 0; bucket < RADIX_BUCKET_COUNT; bucket++)
    packed_counts[bucket] = 0;

  input_cursor = words_;
  remaining    = count_;
  while(remaining >= RADIX_UNROLLED_WORDS)
    {
      word = *input_cursor++;
      packed_counts[word & RADIX_BYTE_MASK]++;
      packed_counts[(word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK] += RADIX_HIGH_COUNT_UNIT;
      word = *input_cursor++;
      packed_counts[word & RADIX_BYTE_MASK]++;
      packed_counts[(word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK] += RADIX_HIGH_COUNT_UNIT;
      word = *input_cursor++;
      packed_counts[word & RADIX_BYTE_MASK]++;
      packed_counts[(word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK] += RADIX_HIGH_COUNT_UNIT;
      word = *input_cursor++;
      packed_counts[word & RADIX_BYTE_MASK]++;
      packed_counts[(word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK] += RADIX_HIGH_COUNT_UNIT;
      remaining -= RADIX_UNROLLED_WORDS;
    }
  while(remaining > 0)
    {
      word = *input_cursor++;
      packed_counts[word & RADIX_BYTE_MASK]++;
      packed_counts[(word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK] += RADIX_HIGH_COUNT_UNIT;
      remaining--;
    }

  output_cursor = scratch_;
  for(bucket = RADIX_BUCKET_COUNT - 1; bucket >= 0; bucket--)
    {
      bucket_cursors[bucket] = output_cursor;
      output_cursor += (packed_counts[bucket] & RADIX_LOW_COUNT_MASK);
    }
  index = 0;
  do
    {
      word                      = words_[index];
      bucket                    = (int32) (word & RADIX_BYTE_MASK);
      *bucket_cursors[bucket]++ = word;
      index++;
    }
  while(index < count_);

  output_cursor = words_;
  for(bucket = RADIX_BUCKET_COUNT - 1; bucket >= 0; bucket--)
    {
      bucket_cursors[bucket] = output_cursor;
      output_cursor += (packed_counts[bucket] >> RADIX_HIGH_COUNT_SHIFT);
    }
  index = 0;
  do
    {
      word                      = scratch_[index];
      bucket                    = (int32) ((word >> RADIX_HIGH_BYTE_SHIFT) & RADIX_BYTE_MASK);
      *bucket_cursors[bucket]++ = word;
      index++;
    }
  while(index < count_);
}
