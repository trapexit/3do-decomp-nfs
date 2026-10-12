#include "simulation_resource_cleanup.h"

#include "bit_utilities.h"
#include "bitmap_clip.h"
#include "driving_warning_display.h"
#include "fixed_math.h"
#include "game_resource_cleanup.h"
#include "memory.h"
#include "packed_data_directory.h"
#include "pointer_array.h"
#include "shape_lookup.h"
#include "simulation_common_art.h"
#include "tire_smoke.h"
#include "vehicle_render_resource_selection.h"
#include "vehicle_runtime_pool.h"

#include "mem.h"

#include "stddef.h"

#define RENDER_MEMORY_FLAGS 0x20
#define TRANSFORMED_COORDINATE_BUFFER_BYTES 3000
#define SCREEN_COORDINATE_BUFFER_BYTES 2000
#define RENDER_DISTANCE_BUFFER_BYTES 600
#define RENDER_SORT_ORDER_BUFFER_BYTES 600
#define PRIMARY_RENDER_DEPTH_LIMIT 150
#define SECONDARY_RENDER_DEPTH_LIMIT 70
#define CAR_RENDER_DISTANCE_THRESHOLD_0 0x7A80
#define CAR_RENDER_DISTANCE_THRESHOLD_1 0x2D00
#define CAR_RENDER_DISTANCE_THRESHOLD_2 0x1900
#define CAR_RENDER_DISTANCE_THRESHOLD_3 0x0F00
#define CAR_RENDER_DISTANCE_THRESHOLD_4 0x0780
#define CAR_RENDER_DISTANCE_INDEX_2 2
#define CAR_RENDER_DISTANCE_INDEX_3 3
#define CAR_RENDER_DISTANCE_INDEX_4 4
#define OBJECT_RENDER_DISTANCE_THRESHOLD_0 24000
#define OBJECT_RENDER_DISTANCE_THRESHOLD_1 2400
#define OBJECT_RENDER_DISTANCE_THRESHOLD_2 800
#define OBJECT_RENDER_DISTANCE_INDEX_2 2
#define RENDER_RESOURCE_SHAPE_REFERENCE_BYTES 8
#define RENDER_RESOURCE_SHAPE_TABLE_BYTES 0x28
#define RENDER_RESOURCE_SHAPE_TABLE_INDEX_OFFSET 0x10
#define RENDER_RESOURCE_SHAPE_TABLE_OFFSET 0x18
#define RENDER_RESOURCE_SHAPE_REFERENCE_OFFSET 4
#define RENDER_RESOURCE_SHAPE_LIMIT_SCALE 8
#define RENDER_RESOURCE_SHAPE_METADATA_WORD 1
#define RENDER_RESOURCE_SHAPE_HIGH_BITS_SHIFT 28
#define RENDER_RESOURCE_SHAPE_LOW_BITS_SHIFT 12
#define RENDER_RESOURCE_SHAPE_HIGH_FIELD_LEFT_SHIFT 4
#define RENDER_RESOURCE_SHAPE_HIGH_FIELD_RIGHT_SHIFT 20
#define RENDER_RESOURCE_SHAPE_LOW_FIELD_SHIFT 20
#define RENDER_RESOURCE_SHAPE_NIBBLE_MASK 0x0F
#define RENDER_RESOURCE_SHAPE_HIGH_BITS_MASK 0x0FFFFFFF
#define RENDER_RESOURCE_SHAPE_LOW_BITS_MASK 0xFFFF0FFF
#define RENDER_RESOURCE_MEMORY_FLAGS 0x20
#define RENDER_RESOURCE_SORT_EXTRA_VALUES 2
#define RENDER_RESOURCE_SHAPE_NAME_PRIMARY "!ori"
#define RENDER_RESOURCE_SHAPE_NAME_ALTERNATE "!ORI"
#define RENDER_RESOURCE_SHAPE_POINTER_NAME "shpptr3D"
#define OBJECT_RENDER_SORT_ORDER_NAME "3DObjSortOrder"

void                *g_TRANSFORMED_COORDINATE_BUFFER;
void                *g_SCREEN_COORDINATE_BUFFER;
void                *g_RENDER_DISTANCE_BUFFER;
void                *g_RENDER_SORT_ORDER_BUFFER;
int32                g_PRIMARY_RENDER_DEPTH_LIMIT   = PRIMARY_RENDER_DEPTH_LIMIT;
int32                g_SECONDARY_RENDER_DEPTH_LIMIT = PRIMARY_RENDER_DEPTH_LIMIT;
int32                g_CAR_RENDER_DISTANCE_THRESHOLDS[CAR_RENDER_DISTANCE_THRESHOLD_COUNT];
int32                g_OBJECT_RENDER_DISTANCE_THRESHOLDS[OBJECT_RENDER_DISTANCE_THRESHOLD_COUNT];
uint8                g_CAR_RENDER_DISTANCE_RESERVED_STATE[0x64];
uint8                g_OBJECT_RENDER_DISTANCE_RESERVED_STATE[0x6C];
RenderResourceRecord g_OBJECT_RENDER_RESOURCE_RECORDS[OBJECT_RENDER_RESOURCE_RECORD_COUNT];
RenderResourceRecord g_CAR_RENDER_RESOURCE_RECORDS[CAR_RENDER_RESOURCE_RECORD_COUNT];

typedef char RenderResourceRecordSize[(sizeof(RenderResourceRecord) == 0x218) ? 1 : -1];
typedef char
  RenderResourceObjectSourceOffset[(offsetof(RenderResourceRecord, object_sources) == 0x00) ? 1 :
                                                                                              -1];
typedef char RenderResourceObjectOffset[(offsetof(RenderResourceRecord, objects) == 0x0C) ? 1 : -1];
typedef char RenderResourceShapePointerOffset
  [(offsetof(RenderResourceRecord, shape_pointer_allocations) == 0x18) ? 1 : -1];
typedef char RenderResourceShapeCollectionOffset
  [(offsetof(RenderResourceRecord, shape_collections) == 0x24) ? 1 : -1];
typedef char
  RenderResourceSortOrderOffset[(offsetof(RenderResourceRecord, sort_keys) == 0x3C) ? 1 : -1];
typedef char RenderResourceInitializedOffset
  [(offsetof(RenderResourceRecord, sort_refresh_countdown) == 0x44) ? 1 : -1];
typedef char
  RenderResourceDepthExtentOffset[(offsetof(RenderResourceRecord, depth_extent) == 0x4C) ? 1 : -1];
typedef char
  RenderResourceWidthExtentOffset[(offsetof(RenderResourceRecord, width_extent) == 0x50) ? 1 : -1];
typedef char
  RenderResourceHeightExtentOffset[(offsetof(RenderResourceRecord, height_extent) == 0x54) ? 1 :
                                                                                             -1];
typedef char RenderResourceHighDetailBatchOffset
  [(offsetof(RenderResourceRecord, high_detail_batches) == 0x74) ? 1 : -1];
typedef char RenderResourceHighShapeIndexOffset
  [(offsetof(RenderResourceRecord, high_detail_shape_indices) == 0xA4) ? 1 : -1];
typedef char RenderResourceHighShapeVariantOffset
  [(offsetof(RenderResourceRecord, high_detail_shape_variants) == 0xB4) ? 1 : -1];
typedef char RenderResourceDistantSpriteOffset
  [(offsetof(RenderResourceRecord, distant_sprite_resources) == 0xC8) ? 1 : -1];
typedef char
  RenderResourceMapShapeOffset[(offsetof(RenderResourceRecord, map_shape) == 0xD0) ? 1 : -1];
typedef char RenderResourceTireSmokeIndexOffset
  [(offsetof(RenderResourceRecord, tire_smoke_vertex_indices) == 0xD4) ? 1 : -1];
typedef char
  RenderResourcePaletteShapeOffset[(offsetof(RenderResourceRecord, palette_shapes) == 0xE0) ? 1 :
                                                                                              -1];
typedef char RenderResourcePaletteShapeCountOffset
  [(offsetof(RenderResourceRecord, palette_shape_counts) == 0x110) ? 1 : -1];
typedef char RenderResourceActivePaletteShapeOffset
  [(offsetof(RenderResourceRecord, active_palette_shape) == 0x11C) ? 1 : -1];
typedef char RenderResourceAnimatedIndexOffset
  [(offsetof(RenderResourceRecord, animated_shape_indices) == 0x120) ? 1 : -1];
typedef char RenderResourceAnimatedShapeOffset
  [(offsetof(RenderResourceRecord, animated_shapes) == 0x150) ? 1 : -1];
typedef char RenderResourceBrakeLightIndexOffset
  [(offsetof(RenderResourceRecord, brake_light_shape_indices) == 0x1EC) ? 1 : -1];
typedef char RenderResourceBrakeLightShapeOffset
  [(offsetof(RenderResourceRecord, brake_light_shapes) == 0x1FC) ? 1 : -1];

static int32
_get_render_shape_reference(const void *mapping_, int32 object_shape_index_);
void
normalize_render_shape_metadata(uint8 *shape_);

void
initialize_render_distance_thresholds(void)
{
  g_CAR_RENDER_DISTANCE_THRESHOLDS[0]                           = CAR_RENDER_DISTANCE_THRESHOLD_0;
  g_CAR_RENDER_DISTANCE_THRESHOLDS[1]                           = CAR_RENDER_DISTANCE_THRESHOLD_1;
  g_CAR_RENDER_DISTANCE_THRESHOLDS[CAR_RENDER_DISTANCE_INDEX_2] = CAR_RENDER_DISTANCE_THRESHOLD_2;
  g_CAR_RENDER_DISTANCE_THRESHOLDS[CAR_RENDER_DISTANCE_INDEX_3] = CAR_RENDER_DISTANCE_THRESHOLD_3;
  g_CAR_RENDER_DISTANCE_THRESHOLDS[CAR_RENDER_DISTANCE_INDEX_4] = CAR_RENDER_DISTANCE_THRESHOLD_4;
  g_OBJECT_RENDER_DISTANCE_THRESHOLDS[0] = OBJECT_RENDER_DISTANCE_THRESHOLD_0;
  g_OBJECT_RENDER_DISTANCE_THRESHOLDS[1] = OBJECT_RENDER_DISTANCE_THRESHOLD_1;
  g_OBJECT_RENDER_DISTANCE_THRESHOLDS[OBJECT_RENDER_DISTANCE_INDEX_2] =
    OBJECT_RENDER_DISTANCE_THRESHOLD_2;
}

void
initialize_simulation_render_resources(void)
{
  RenderResourceRecord *record;
  int32                 record_index;
  int32                 shape_index;

  initialize_render_distance_thresholds();
  for(record_index = 0; record_index < OBJECT_RENDER_RESOURCE_RECORD_COUNT; record_index += 1)
    {
      record            = &g_OBJECT_RENDER_RESOURCE_RECORDS[record_index];
      record->sort_keys = NULL;
      for(shape_index = 0; shape_index < RENDER_SHAPE_POINTER_COUNT; shape_index += 1)
        record->shape_pointer_allocations[shape_index] = NULL;
    }
  for(record_index = 0; record_index < CAR_RENDER_RESOURCE_RECORD_COUNT; record_index += 1)
    {
      g_CAR_RENDER_RESOURCE_RECORDS[record_index].wrapper_file = NULL;
    }
  g_TRANSFORMED_COORDINATE_BUFFER = allocate_named_memory("transformedcoord",
                                                          TRANSFORMED_COORDINATE_BUFFER_BYTES,
                                                          RENDER_MEMORY_FLAGS);
  g_SCREEN_COORDINATE_BUFFER =
    allocate_named_memory("ScreenCOORD", SCREEN_COORDINATE_BUFFER_BYTES, RENDER_MEMORY_FLAGS);
  g_RENDER_DISTANCE_BUFFER =
    allocate_named_memory("Distance", RENDER_DISTANCE_BUFFER_BYTES, RENDER_MEMORY_FLAGS);
  g_RENDER_SORT_ORDER_BUFFER =
    allocate_named_memory("SortOrder", RENDER_SORT_ORDER_BUFFER_BYTES, RENDER_MEMORY_FLAGS);
  g_VEHICLE_ROAD_ANIMATION_STATE.quad_view_points   = (Vector3 *) g_TRANSFORMED_COORDINATE_BUFFER;
  g_VEHICLE_ROAD_ANIMATION_STATE.quad_screen_points = (Point2 *) g_SCREEN_COORDINATE_BUFFER;
  g_VEHICLE_ROAD_ANIMATION_STATE.polygon_depth_sort_scratch = (uint32 *) g_RENDER_DISTANCE_BUFFER;
  g_PRIMARY_RENDER_DEPTH_LIMIT                              = PRIMARY_RENDER_DEPTH_LIMIT;
  g_SECONDARY_RENDER_DEPTH_LIMIT                            = SECONDARY_RENDER_DEPTH_LIMIT;
  initialize_simulation_common_art_cels();
}

void
initialize_render_resource_record(void *resource_, RenderResourceRecord *record_)
{
  uint8        *shape;
  const void   *object_source;
  const void   *previous_source;
  ThreeDObject *object;
  ThreeDObject *previous_object;
  void         *shape_collection;
  void         *shape_mapping;
  int32         reference;
  int32         shape_group_index;
  int32         shape_index;

  previous_source = NULL;
  previous_object = NULL;
  for(shape_group_index = 0; shape_group_index < RENDER_SHAPE_POINTER_COUNT; shape_group_index += 1)
    {
      object_source =
        get_nested_pointer_array_element((void *const *) resource_, shape_group_index, 0);
      if(object_source == previous_source)
        {
          record_->objects[shape_group_index] = previous_object;
          continue;
        }

      object = fixup_three_d_object((ThreeDObject *) object_source);
      shape_collection =
        get_nested_pointer_array_element((void *const *) resource_, shape_group_index, 1);
      record_->shape_pointer_allocations[shape_group_index] =
        (uint8 **) allocate_named_memory(RENDER_RESOURCE_SHAPE_POINTER_NAME,
                                         object->shape_count * sizeof(uint8 *),
                                         RENDER_RESOURCE_MEMORY_FLAGS);
      shape_mapping =
        locate_shape(shape_collection, (const uint32 *) RENDER_RESOURCE_SHAPE_NAME_PRIMARY);
      if(shape_mapping == NULL)
        {
          shape_mapping =
            locate_shape(shape_collection, (const uint32 *) RENDER_RESOURCE_SHAPE_NAME_ALTERNATE);
        }
      if(shape_mapping != NULL)
        {
          for(shape_index = 0; shape_index < object->shape_count; shape_index += 1)
            {
              reference = _get_render_shape_reference(shape_mapping, shape_index);
              if((reference < 0) ||
                 (reference >= (object->shape_count * RENDER_RESOURCE_SHAPE_LIMIT_SCALE)))
                reference = 0;
              shape = (uint8 *) get_packed_directory_entry(shape_collection, reference);
              record_->shape_pointer_allocations[shape_group_index][shape_index] = shape;
              normalize_render_shape_metadata(shape);
            }
        }
      record_->objects[shape_group_index] = object;
      previous_source                     = object_source;
      previous_object                     = object;
    }
}

void
initialize_object_render_resources(void *resources_)
{
  RenderResourceRecord *record;
  ThreeDObject         *object;
  int32                 object_count;
  int32                 object_index;

  object_count = *((const uint8 *) resources_ - 1);
  for(object_index = 0; object_index < object_count; object_index += 1)
    {
      record = &g_OBJECT_RENDER_RESOURCE_RECORDS[object_index];
      initialize_render_resource_record(
        get_pointer_array_element((void *const *) resources_, object_index),
        record);
      object            = record->objects[RENDER_SHAPE_POINTER_COUNT - 1];
      record->sort_keys = (uint32 *) allocate_named_memory(
        OBJECT_RENDER_SORT_ORDER_NAME,
        (object->shape_count + RENDER_RESOURCE_SORT_EXTRA_VALUES) * sizeof(int32),
        RENDER_RESOURCE_MEMORY_FLAGS);
      fill_fixed16_ramp(object->shape_count, (int32 *) record->sort_keys);
      record->sort_refresh_countdown = 1;
    }
}

void
release_object_render_resources(void)
{
  RenderResourceRecord *record;
  int32                 record_index;
  int32                 shape_index;

  for(record_index = 0; record_index < OBJECT_RENDER_RESOURCE_RECORD_COUNT; record_index += 1)
    {
      record = &g_OBJECT_RENDER_RESOURCE_RECORDS[record_index];
      if(record->sort_keys != NULL)
        {
          free_memory(record->sort_keys);
          record->sort_keys = NULL;
        }
      for(shape_index = 0; shape_index < RENDER_SHAPE_POINTER_COUNT; shape_index += 1)
        {
          if(record->shape_pointer_allocations[shape_index] != NULL)
            {
              free_memory(record->shape_pointer_allocations[shape_index]);
              record->shape_pointer_allocations[shape_index] = NULL;
            }
        }
    }
}

void
release_car_render_resources(void)
{
  RenderResourceRecord *record;
  int32                 record_index;
  int32                 shape_index;

  for(record_index = 0; record_index < CAR_RENDER_RESOURCE_RECORD_COUNT; record_index += 1)
    {
      record = &g_CAR_RENDER_RESOURCE_RECORDS[record_index];
      if(record->wrapper_file != NULL)
        free_memory(record->wrapper_file);
      for(shape_index = 0; shape_index < RENDER_SHAPE_POINTER_COUNT; shape_index += 1)
        {
          if(record->shape_pointer_allocations[shape_index] != NULL)
            free_memory(record->shape_pointer_allocations[shape_index]);
        }
      if(record->sort_keys != NULL)
        free_memory(record->sort_keys);
    }
}

void
shutdown_simulation_render_resources(void)
{
  free_memory(g_TRANSFORMED_COORDINATE_BUFFER);
  free_memory(g_SCREEN_COORDINATE_BUFFER);
  free_memory(g_RENDER_DISTANCE_BUFFER);
  free_memory(g_RENDER_SORT_ORDER_BUFFER);
  release_car_render_resources();
}

void
shutdown_simulation_resources(void)
{
  ScavengeMem();
  shutdown_driving_warning_display();
  ScavengeMem();
  clear_tire_smoke_cel_cache();
  ScavengeMem();
  release_game_resource_allocations();
  ScavengeMem();
  shutdown_simulation_render_resources();
  shutdown_vehicle_runtime_pool();
  ScavengeMem();
  close_bitmap_display_items();
  ScavengeMem();
}

static int32
_get_render_shape_reference(const void *mapping_, int32 object_shape_index_)
{
  const uint8 *mapping;
  const uint8 *reference_table;
  int32        table_index;

  mapping         = (const uint8 *) mapping_;
  table_index     = *(const int32 *) (mapping + RENDER_RESOURCE_SHAPE_TABLE_INDEX_OFFSET);
  reference_table = mapping + RENDER_RESOURCE_SHAPE_TABLE_OFFSET +
                    (table_index * RENDER_RESOURCE_SHAPE_TABLE_BYTES);
  return *(const int32 *) (reference_table + RENDER_RESOURCE_SHAPE_REFERENCE_OFFSET +
                           (object_shape_index_ * RENDER_RESOURCE_SHAPE_REFERENCE_BYTES));
}

void
normalize_render_shape_metadata(uint8 *shape_)
{
  uint32 metadata;
  int32  field;
  int32  highest_bit;

  metadata = ((uint32 *) shape_)[RENDER_RESOURCE_SHAPE_METADATA_WORD];
  if((metadata >> RENDER_RESOURCE_SHAPE_HIGH_BITS_SHIFT) != 0)
    return;

  field       = (int32) (metadata << RENDER_RESOURCE_SHAPE_HIGH_FIELD_LEFT_SHIFT) >>
                RENDER_RESOURCE_SHAPE_HIGH_FIELD_RIGHT_SHIFT;
  highest_bit = get_highest_set_bit_index((uint32) field);
  metadata    = (metadata & RENDER_RESOURCE_SHAPE_HIGH_BITS_MASK) |
                ((uint32) (highest_bit & RENDER_RESOURCE_SHAPE_NIBBLE_MASK)
                 << RENDER_RESOURCE_SHAPE_HIGH_BITS_SHIFT);
  field       = (int32) (metadata << RENDER_RESOURCE_SHAPE_LOW_FIELD_SHIFT) >>
                RENDER_RESOURCE_SHAPE_LOW_FIELD_SHIFT;
  highest_bit = get_highest_set_bit_index((uint32) field);
  metadata    = (metadata & RENDER_RESOURCE_SHAPE_LOW_BITS_MASK) |
                ((uint32) (highest_bit & RENDER_RESOURCE_SHAPE_NIBBLE_MASK)
                 << RENDER_RESOURCE_SHAPE_LOW_BITS_SHIFT);
  ((uint32 *) shape_)[RENDER_RESOURCE_SHAPE_METADATA_WORD] = metadata;
}
