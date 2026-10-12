#include "vehicle_render_resource_selection.h"

#include "camera_io.h"
#include "disk_stream_cleanup.h"
#include "fixed_math.h"
#include "headered_file.h"
#include "memory.h"
#include "packed_data_directory.h"
#include "pointer_array.h"
#include "random_number.h"
#include "resource_partition.h"
#include "shape_lookup.h"
#include "simulation_resource_cleanup.h"
#include "string_utilities.h"
#include "text_line_reader.h"
#include "vehicle_identifiers.h"
#include "vehicle_shape_tags.h"
#include "vehicle_simulation_state.h"

#include "blockfile.h"
#include "hardware.h"
#include "operamath.h"

#include "stddef.h"
#include "string.h"

#define VEHICLE_RESOURCE_SLOT_PLAYER 0
#define VEHICLE_RESOURCE_SLOT_OPPONENT 1
#define VEHICLE_RESOURCE_SLOT_POLICE 2
#define VEHICLE_RESOURCE_SLOT_TRAFFIC_FIRST 3
#define VEHICLE_RESOURCE_TRAFFIC_SLOT_COUNT 4
#define VEHICLE_RESOURCE_SELECTION_COUNT 20
#define VEHICLE_RESOURCE_TRAFFIC_IDENTIFIER_FIRST 13
#define VEHICLE_RESOURCE_TRAFFIC_IDENTIFIER_COUNT 12
#define VEHICLE_IDENTIFIER_DEFAULT 0
#define VEHICLE_IDENTIFIER_OVERRIDE 8
#define VEHICLE_IDENTIFIER_POLICE 9
#define VEHICLE_SIMULATION_MODE_DEFAULT_OPPONENT 8
#define VEHICLE_RESOURCE_PATH_BYTES 100
#define VEHICLE_RESOURCE_DYNAMIC_DATA_BYTES 0x1DC
#define VEHICLE_RESOURCE_MEMORY_FLAGS 0x20
#define VEHICLE_RESOURCE_WRAPPER_PATH_PREFIX "DriveData/CarData/"
#define VEHICLE_RESOURCE_WRAPPER_PATH_SUFFIX ".wrapFam"
#define VEHICLE_RESOURCE_SORT_ORDER_NAME "CarSortOrder"
#define VEHICLE_RESOURCE_DYNAMIC_NAME "TdDyn"
#define VEHICLE_RESOURCE_DYNAMIC_PATH_SUFFIX ".TdDyn"
#define VEHICLE_RESOURCE_CACHE_SEPARATOR "/"
#define VEHICLE_RESOURCE_DYNAMIC_FILE_SUFFIX ".TdDynBIN"
#define VEHICLE_RESOURCE_SORT_ORDER_EXTRA_VALUES 2
#define VEHICLE_RESOURCE_DYNAMIC_INITIALIZED_OFFSET 4
#define VEHICLE_RESOURCE_DYNAMIC_OBJECT_INDEX 3
#define VEHICLE_RESOURCE_OBJECT_LINK_WORD_COUNT 3
#define VEHICLE_RESOURCE_DETAIL_COUNT 3
#define VEHICLE_RESOURCE_HIGH_SHAPE_INDEX_COUNT 4
#define VEHICLE_RESOURCE_WHEEL_VARIANT_COUNT 3
#define VEHICLE_RESOURCE_AUXILIARY_SHAPE_COUNT 4
#define VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT 4
#define VEHICLE_RESOURCE_ANIMATION_FRAME_FIRST 1
#define VEHICLE_RESOURCE_ANIMATION_FRAME_COUNT 2
#define VEHICLE_RESOURCE_ANIMATION_DETAIL_STRIDE 12
#define VEHICLE_RESOURCE_ANIMATION_GROUP_STRIDE 3
#define VEHICLE_RESOURCE_BRAKE_DETAIL_COUNT 2
#define VEHICLE_RESOURCE_BRAKE_TAG_COUNT 2
#define VEHICLE_RESOURCE_SMOKE_TAG "smok"
#define VEHICLE_RESOURCE_HIGH_SHAPE_TAG_FIRST 3
#define VEHICLE_RESOURCE_WHEEL_VARIANT_TAG_FIRST 7
#define VEHICLE_RESOURCE_AUXILIARY_TAG_FIRST 10
#define VEHICLE_RESOURCE_ANIMATION_TAG_FIRST 14
#define VEHICLE_RESOURCE_HIGH_ANIMATION_TAG_FIRST 18
#define VEHICLE_RESOURCE_ANIMATION_FRAME_TAG_FIRST 22
#define VEHICLE_RESOURCE_BRAKE_TAG_FIRST 30
#define VEHICLE_RESOURCE_BRAKE_VARIANT_TAG 32
#define VEHICLE_RESOURCE_AUXILIARY_DISTANT_SECOND 2
#define VEHICLE_RESOURCE_ANIMATION_PRIMARY_OVERRIDE 2
#define VEHICLE_RESOURCE_ANIMATION_SECONDARY_OVERRIDE 4
#define VEHICLE_RESOURCE_SHAPE_MAPPING_PRIMARY "!ori"
#define VEHICLE_RESOURCE_SHAPE_MAPPING_ALTERNATE "!ORI"
#define VEHICLE_RESOURCE_SHAPE_POINTER_NAME "shapeptr"
#define VEHICLE_RESOURCE_PALETTE_SHAPE_PREFIX "plt0"
#define VEHICLE_RESOURCE_PALETTE_SHAPE_COUNT 4
#define VEHICLE_RESOURCE_PALETTE_TAG_BYTES 5
#define VEHICLE_RESOURCE_PALETTE_DIGIT_OFFSET 3
#define VEHICLE_RESOURCE_PARTITION_DETAIL_COUNT 2
#define VEHICLE_RESOURCE_PARTITION_TAG_COUNT 3
#define VEHICLE_RESOURCE_COORDINATE_SHIFT 9
#define VEHICLE_RESOURCE_PARTITION_VALUE_SHIFT 16
#define VEHICLE_RESOURCE_SHAPE_TABLE_INDEX_OFFSET 0x10
#define VEHICLE_RESOURCE_SHAPE_TABLE_OFFSET 0x18
#define VEHICLE_RESOURCE_SHAPE_TABLE_BYTES 0x28
#define VEHICLE_RESOURCE_SHAPE_REFERENCE_OFFSET 4
#define VEHICLE_RESOURCE_SHAPE_REFERENCE_BYTES 8
#define VEHICLE_RESOURCE_SHAPE_REFERENCE_LIMIT_SCALE 8
#define VEHICLE_RESOURCE_WHEEL_SHAPE_COUNT 4
#define VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT 4
#define VEHICLE_RESOURCE_WHEEL_SHAPE_VERTEX_OFFSET 8
#define VEHICLE_RESOURCE_WHEEL_SHAPE_ENTRY_BYTES 0x18
#define VEHICLE_RESOURCE_HIGH_DETAIL_INDEX 2
#define VEHICLE_RESOURCE_WHEEL_POSITIVE_Z_POSITIVE_Y 0
#define VEHICLE_RESOURCE_WHEEL_POSITIVE_Z_NEGATIVE_Y 1
#define VEHICLE_RESOURCE_WHEEL_NEGATIVE_Z_POSITIVE_Y 2
#define VEHICLE_RESOURCE_WHEEL_NEGATIVE_Z_NEGATIVE_Y 3
#define VEHICLE_RESOURCE_WHEEL_REFERENCE_X_VERTEX 0
#define VEHICLE_RESOURCE_WHEEL_REFERENCE_Y_VERTEX 1
#define VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_FIRST_VERTEX 1
#define VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_SECOND_VERTEX 3
#define VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_DIVISOR 2
#define VEHICLE_DYNAMIC_TEXT_LINE_BYTES 128
#define VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT 127
#define VEHICLE_DYNAMIC_IGNORED_HEADER_VALUE_COUNT 3
#define VEHICLE_DYNAMIC_CURVE_VALUE_MINIMUM 0
#define VEHICLE_DYNAMIC_CURVE_VALUE_MAXIMUM 0xFFFF
#define VEHICLE_DYNAMIC_INTEGER_SHIFT 16
#define VEHICLE_DYNAMIC_TOKEN_SCALE 0x71C4
#define VEHICLE_DYNAMIC_LINK_COUNT 3

typedef struct VehicleResourceShapeEntry
{
  uint8 reserved_00[VEHICLE_RESOURCE_WHEEL_SHAPE_VERTEX_OFFSET];
  int32 vertex_indices[VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT];
} VehicleResourceShapeEntry;

typedef char VehicleResourceShapeEntrySize
  [(sizeof(VehicleResourceShapeEntry) == VEHICLE_RESOURCE_WHEEL_SHAPE_ENTRY_BYTES) ? 1 : -1];
typedef char
  VehicleRoadWheelShapeRecordSize[(sizeof(VehicleRoadWheelShapeRecord) == 0x4C) ? 1 : -1];
typedef char VehicleDynamicRenderObjectSize
  [(sizeof(VehicleDynamicRenderObject) == VEHICLE_RESOURCE_DYNAMIC_DATA_BYTES) ? 1 : -1];
typedef char
  VehicleDynamicCurveOffset[(offsetof(VehicleDynamicRenderObject, curve_values) == 0x2C) ? 1 : -1];
typedef char VehicleDynamicScaledTokenOffset
  [(offsetof(VehicleDynamicRenderObject, scaled_token_values) == 0x1BC) ? 1 : -1];
typedef char VehicleDynamicTrailingFixedOffset
  [(offsetof(VehicleDynamicRenderObject, trailing_fixed_value) == 0x1D4) ? 1 : -1];
typedef char VehicleDynamicTrailingIntegerOffset
  [(offsetof(VehicleDynamicRenderObject, trailing_integer_value) == 0x1D8) ? 1 : -1];
typedef char VehicleRoadDynamicDataEnabledOffset
  [(offsetof(VehicleRoadAnimationState, dynamic_data_enabled) == 0x1B0) ? 1 : -1];

VehicleRoadAnimationState        g_VEHICLE_ROAD_ANIMATION_STATE;
extern VehicleRoadAnimationState g_VEHICLE_ROAD_ANIMATION_STATE_CALLBACKS;
extern VehicleRoadAnimationState g_VEHICLE_ROAD_ANIMATION_STATE_QUAD_PADDING;
extern VehicleRoadAnimationState g_VEHICLE_ROAD_ANIMATION_STATE_PROJECTION_STORAGE;
extern VehicleRoadAnimationState g_VEHICLE_ROAD_ANIMATION_STATE_ANIMATION_COUNTERS;
VehicleDynamicRenderObject      *g_VEHICLE_DYNAMIC_RENDER_OBJECTS[CAR_RENDER_RESOURCE_RECORD_COUNT];

void
load_vehicle_dynamic_resource(void *destination_, const char *path_)
{
  VehicleDynamicRenderObject *destination;
  Stream                     *stream;
  const char                 *cursor;
  char                        line[VEHICLE_DYNAMIC_TEXT_LINE_BYTES];
  char                        token[VEHICLE_DYNAMIC_TEXT_LINE_BYTES];
  int32                       curve_index;
  int32                       link_index;
  int32                       token_index;
  int32                       value;

  destination = (VehicleDynamicRenderObject *) destination_;
  stream      = open_disk_stream_default((char *) path_);

  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  destination->primary_header_value = parse_fixed16_decimal(line);
  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  destination->secondary_header_value = parse_fixed16_decimal(line);

  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  cursor = (const char *) line;
  for(token_index = 0; token_index < VEHICLE_DYNAMIC_IGNORED_HEADER_VALUE_COUNT; token_index += 1)
    {
      copy_text_token(token, cursor);
      parse_fixed16_decimal(token);
      cursor = skip_text_token(cursor);
    }

  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  cursor = (const char *) line;
  for(token_index = 0; token_index < VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT; token_index += 1)
    {
      copy_text_token(token, cursor);
      destination->scaled_token_values[token_index] =
        MulSF16(parse_fixed16_decimal(token), VEHICLE_DYNAMIC_TOKEN_SCALE);
      cursor = skip_text_token(cursor);
    }

  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  destination->trailing_fixed_value = parse_fixed16_decimal(line);
  read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
  destination->trailing_integer_value =
    parse_fixed16_decimal(line) >> VEHICLE_DYNAMIC_INTEGER_SHIFT;

  for(token_index = 0; token_index < VEHICLE_DYNAMIC_CONFIGURATION_VALUE_COUNT; token_index += 1)
    {
      read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
      destination->configuration_values[token_index] = parse_fixed16_decimal(line);
    }

  for(curve_index = 0; curve_index < VEHICLE_DYNAMIC_CURVE_VALUE_COUNT; curve_index += 1)
    {
      read_noncomment_text_line(line, VEHICLE_DYNAMIC_TEXT_LINE_READ_LIMIT, stream);
      value = parse_fixed16_decimal(line);
      if(value > VEHICLE_DYNAMIC_CURVE_VALUE_MAXIMUM)
        value = VEHICLE_DYNAMIC_CURVE_VALUE_MAXIMUM;
      else if(value < VEHICLE_DYNAMIC_CURVE_VALUE_MINIMUM)
        value = VEHICLE_DYNAMIC_CURVE_VALUE_MINIMUM;
      destination->curve_values[curve_index] = value;
    }

  close_disk_stream_and_return_zero(stream);
  for(link_index = 0; link_index < VEHICLE_DYNAMIC_LINK_COUNT; link_index += 1)
    destination->links[link_index] = 0;
}

void
calculate_vehicle_resource_extents(RenderResourceRecord *record_, int32 coordinate_shift_)
{
  const ThreeDObject *object;
  const Vector3      *vertex;
  int32               maximum_x;
  int32               maximum_y;
  int32               maximum_z;
  int32               minimum_x;
  int32               minimum_y;
  int32               minimum_z;
  int32               vertex_index;

  minimum_x = 0;
  minimum_y = 0;
  minimum_z = 0;
  maximum_x = 0;
  maximum_y = 0;
  maximum_z = 0;
  object    = record_->objects[RENDER_SHAPE_POINTER_COUNT - 1];
  vertex    = (const Vector3 *) object->material_table;
  for(vertex_index = 0; vertex_index < object->material_count; vertex_index += 1)
    {
      if(vertex->x < minimum_x)
        minimum_x = vertex->x;
      if(vertex->y < minimum_y)
        minimum_y = vertex->y;
      if(vertex->z < minimum_z)
        minimum_z = vertex->z;
      if(vertex->x > maximum_x)
        maximum_x = vertex->x;
      if(vertex->y > maximum_y)
        maximum_y = vertex->y;
      if(vertex->z > maximum_z)
        maximum_z = vertex->z;
      vertex += 1;
    }

  record_->depth_extent  = (maximum_z - minimum_z) << coordinate_shift_;
  record_->width_extent  = (maximum_x - minimum_x) << coordinate_shift_;
  record_->height_extent = (maximum_y - minimum_y) << coordinate_shift_;
}

void
initialize_player_vehicle_wheel_shapes(RenderResourceRecord *record_)
{
  const VehicleResourceShapeEntry *shape_entry;
  VehicleRoadWheelShapeRecord     *wheel;
  const Vector3                   *vertices;
  const Vector3                   *vertex;
  ThreeDObject                    *object;
  int32                            average_y;
  int32                            average_z;
  int32                            delta_y;
  int32                            delta_z;
  int32                            shape_index;
  int32                            vertex_index;

  if(record_->high_detail_shape_indices[0] < 0)
    return;

  object   = record_->objects[VEHICLE_RESOURCE_HIGH_DETAIL_INDEX];
  vertices = (const Vector3 *) object->material_table;
  for(shape_index = 0; shape_index < VEHICLE_RESOURCE_WHEEL_SHAPE_COUNT; shape_index += 1)
    {
      shape_entry =
        (const VehicleResourceShapeEntry *) (object->shape_table +
                                             (record_->high_detail_shape_indices[shape_index] *
                                              VEHICLE_RESOURCE_WHEEL_SHAPE_ENTRY_BYTES));
      average_y = 0;
      average_z = 0;
      for(vertex_index = 0; vertex_index < VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT; vertex_index += 1)
        {
          vertex = &vertices[shape_entry->vertex_indices[vertex_index]];
          average_y += vertex->y;
          average_z += vertex->z;
        }
      average_y /= VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT;
      average_z /= VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT;

      wheel = &g_VEHICLE_ROAD_WHEEL_SHAPE_STATE.wheels[shape_index];
      for(vertex_index = 0; vertex_index < VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT; vertex_index += 1)
        {
          vertex  = &vertices[shape_entry->vertex_indices[vertex_index]];
          delta_y = vertex->y - average_y;
          delta_z = vertex->z - average_z;
          if((delta_z > 0) && (delta_y > 0))
            wheel->vertex_indices[VEHICLE_RESOURCE_WHEEL_POSITIVE_Z_POSITIVE_Y] =
              shape_entry->vertex_indices[vertex_index];
          else if((delta_z > 0) && (delta_y < 0))
            wheel->vertex_indices[VEHICLE_RESOURCE_WHEEL_POSITIVE_Z_NEGATIVE_Y] =
              shape_entry->vertex_indices[vertex_index];
          else if((delta_z < 0) && (delta_y > 0))
            wheel->vertex_indices[VEHICLE_RESOURCE_WHEEL_NEGATIVE_Z_POSITIVE_Y] =
              shape_entry->vertex_indices[vertex_index];
          else if((delta_z < 0) && (delta_y < 0))
            wheel->vertex_indices[VEHICLE_RESOURCE_WHEEL_NEGATIVE_Z_NEGATIVE_Y] =
              shape_entry->vertex_indices[vertex_index];
        }

      for(vertex_index = 0; vertex_index < VEHICLE_RESOURCE_WHEEL_VERTEX_COUNT; vertex_index += 1)
        {
          wheel->vertices[vertex_index] = vertices[wheel->vertex_indices[vertex_index]];
        }
      wheel->reference_position.x = wheel->vertices[VEHICLE_RESOURCE_WHEEL_REFERENCE_X_VERTEX].x;
      wheel->reference_position.y = wheel->vertices[VEHICLE_RESOURCE_WHEEL_REFERENCE_Y_VERTEX].y;
      wheel->reference_position.z =
        (wheel->vertices[VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_FIRST_VERTEX].z +
         wheel->vertices[VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_SECOND_VERTEX].z) /
        VEHICLE_RESOURCE_WHEEL_REFERENCE_Z_DIVISOR;
    }
}

void
initialize_vehicle_render_resource_record(void *wrapper_resource_, RenderResourceRecord *record_)
{
  const uint8                *mapping;
  const uint8                *reference_table;
  const void                 *object_source;
  const void                 *previous_source;
  ThreeDObject               *object;
  ThreeDObject               *previous_object;
  uint8                      *shape;
  void                       *shape_collection;
  void                       *shared_shape_collection;
  RenderResourcePolygonBatch *batches;
  int32                      *partition_values;
  int32                       detail_index;
  int32                       palette_index;
  int32                       partition_id;
  int32                       partition_index;
  int32                       reference;
  int32                       shape_index;
  int32                       value_index;
  char                        palette_tag[VEHICLE_RESOURCE_PALETTE_TAG_BYTES];

  previous_source         = NULL;
  previous_object         = NULL;
  shared_shape_collection = NULL;
  for(detail_index = 0; detail_index < VEHICLE_RESOURCE_DETAIL_COUNT; detail_index += 1)
    {
      object_source = get_headered_nested_pointer_array_element((void *const *) wrapper_resource_,
                                                                detail_index,
                                                                0);
      record_->object_sources[detail_index] = (void *) object_source;
      if(object_source == previous_source)
        {
          record_->objects[detail_index] = previous_object;
          continue;
        }

      object = fixup_three_d_object((ThreeDObject *) object_source);
      shape_collection =
        get_headered_nested_pointer_array_element((void *const *) wrapper_resource_,
                                                  detail_index,
                                                  1);
      shared_shape_collection                  = shape_collection;
      record_->shape_collections[detail_index] = shape_collection;
      record_->shape_pointer_allocations[detail_index] =
        (uint8 **) allocate_named_memory(VEHICLE_RESOURCE_SHAPE_POINTER_NAME,
                                         (uint32) object->shape_count * sizeof(uint8 *),
                                         VEHICLE_RESOURCE_MEMORY_FLAGS);
      mapping =
        (const uint8 *) locate_shape(shape_collection,
                                     (const uint32 *) VEHICLE_RESOURCE_SHAPE_MAPPING_PRIMARY);
      if(mapping == NULL)
        {
          mapping =
            (const uint8 *) locate_shape(shape_collection,
                                         (const uint32 *) VEHICLE_RESOURCE_SHAPE_MAPPING_ALTERNATE);
        }
      if(mapping != NULL)
        {
          reference_table =
            mapping + VEHICLE_RESOURCE_SHAPE_TABLE_OFFSET +
            (*(const int32 *) (mapping + VEHICLE_RESOURCE_SHAPE_TABLE_INDEX_OFFSET) *
             VEHICLE_RESOURCE_SHAPE_TABLE_BYTES);
          for(shape_index = 0; shape_index < object->shape_count; shape_index += 1)
            {
              reference =
                *(const int32 *) (reference_table + VEHICLE_RESOURCE_SHAPE_REFERENCE_OFFSET +
                                  (shape_index * VEHICLE_RESOURCE_SHAPE_REFERENCE_BYTES));
              if((reference < 0) || (reference >= (object->shape_count *
                                                   VEHICLE_RESOURCE_SHAPE_REFERENCE_LIMIT_SCALE)))
                reference = 0;
              shape = (uint8 *) get_packed_directory_entry(shape_collection, (uint32) reference);
              record_->shape_pointer_allocations[detail_index][shape_index] = shape;
              normalize_render_shape_metadata(shape);
            }
        }

      memcpy(palette_tag, VEHICLE_RESOURCE_PALETTE_SHAPE_PREFIX, sizeof(palette_tag));
      record_->palette_shape_counts[detail_index] = 0;
      for(palette_index = 0; palette_index < VEHICLE_RESOURCE_PALETTE_SHAPE_COUNT;
          palette_index += 1)
        {
          palette_tag[VEHICLE_RESOURCE_PALETTE_DIGIT_OFFSET] = (char) ('0' + palette_index);
          shape = (uint8 *) locate_shape(shape_collection, (const uint32 *) palette_tag);
          record_->palette_shapes[detail_index][palette_index] = shape;
          if(shape != NULL)
            record_->palette_shape_counts[detail_index] = palette_index + 1;
        }
      record_->active_palette_shape  = 0;
      record_->objects[detail_index] = object;
      previous_source                = object_source;
      previous_object                = object;
    }

  calculate_vehicle_resource_extents(record_, VEHICLE_RESOURCE_COORDINATE_SHIFT);
  for(detail_index = 0; detail_index < VEHICLE_RESOURCE_PARTITION_DETAIL_COUNT; detail_index += 1)
    {
      if(detail_index == 0)
        batches = record_->high_detail_batches;
      else
        batches = record_->medium_detail_batches;
      object_source = record_->object_sources[(VEHICLE_RESOURCE_DETAIL_COUNT - 1) - detail_index];
      for(partition_index = 0; partition_index < VEHICLE_RESOURCE_PARTITION_TAG_COUNT;
          partition_index += 1)
        {
          partition_id = find_resource_partition_id(object_source,
                                                    g_VEHICLE_SHAPE_TAG_POINTERS[partition_index]);
          if(partition_id < 0)
            {
              batches[partition_index].sort_keys     = NULL;
              batches[partition_index].polygon_count = 0;
              continue;
            }

          partition_values = get_resource_partition_data(object_source,
                                                         partition_id,
                                                         &batches[partition_index].polygon_count);
          batches[partition_index].sort_keys = (uint32 *) partition_values;
          for(value_index = 0; value_index < batches[partition_index].polygon_count;
              value_index += 1)
            {
              partition_values[value_index] <<= VEHICLE_RESOURCE_PARTITION_VALUE_SHIFT;
            }
        }
    }

  setup_vehicle_render_shape_references(record_, shared_shape_collection);
}

void
setup_vehicle_render_shape_references(RenderResourceRecord *record_, void *shared_shape_collection_)
{
  uint8 **shape_pointers;
  uint8  *shape;
  int32   animation_group;
  int32   detail_index;
  int32   frame_index;
  int32   shape_index;
  int32   source_index;
  int32   tag_index;

  for(detail_index = 0; detail_index < VEHICLE_RESOURCE_DETAIL_COUNT; detail_index += 1)
    {
      source_index = (VEHICLE_RESOURCE_DETAIL_COUNT - 1) - detail_index;
      record_->tire_smoke_vertex_indices[detail_index] =
        find_resource_object_tag_index(record_->object_sources[source_index],
                                       VEHICLE_RESOURCE_SMOKE_TAG);
    }

  for(shape_index = 0; shape_index < VEHICLE_RESOURCE_HIGH_SHAPE_INDEX_COUNT; shape_index += 1)
    {
      record_->high_detail_shape_indices[shape_index] = find_resource_shape_tag_index(
        record_->object_sources[VEHICLE_RESOURCE_DETAIL_COUNT - 1],
        g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_RESOURCE_HIGH_SHAPE_TAG_FIRST + shape_index]);
    }

  for(shape_index = 0; shape_index < VEHICLE_RESOURCE_WHEEL_VARIANT_COUNT; shape_index += 1)
    {
      shape = (uint8 *) locate_shape(
        shared_shape_collection_,
        (const uint32 *)
          g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_RESOURCE_WHEEL_VARIANT_TAG_FIRST + shape_index]);
      record_->high_detail_shape_variants[shape_index] = shape;
      if(shape != NULL)
        normalize_render_shape_metadata(shape);
    }
  if(record_->high_detail_shape_indices[0] >= 0)
    {
      record_->high_detail_shape_variants[VEHICLE_RESOURCE_WHEEL_VARIANT_COUNT] =
        record_->shape_pointer_allocations[VEHICLE_RESOURCE_DETAIL_COUNT - 1]
                                          [record_->high_detail_shape_indices[0]];
    }

  for(shape_index = 0; shape_index < VEHICLE_RESOURCE_AUXILIARY_SHAPE_COUNT; shape_index += 1)
    {
      shape = (uint8 *) locate_shape(
        shared_shape_collection_,
        (const uint32 *)
          g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_RESOURCE_AUXILIARY_TAG_FIRST + shape_index]);
      switch(shape_index)
        {
          case 0:
            record_->high_detail_shape_variants[VEHICLE_RESOURCE_WHEEL_VARIANT_COUNT + 1] = shape;
            break;

          case 1:
            record_->distant_sprite_resources[0] = shape;
            break;

          case VEHICLE_RESOURCE_AUXILIARY_DISTANT_SECOND:
            record_->distant_sprite_resources[1] = shape;
            break;

          default:
            record_->map_shape = shape;
            break;
        }
    }

  for(detail_index = 0; detail_index < VEHICLE_RESOURCE_DETAIL_COUNT; detail_index += 1)
    {
      source_index = (VEHICLE_RESOURCE_DETAIL_COUNT - 1) - detail_index;
      for(animation_group = 0; animation_group < VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT;
          animation_group += 1)
        {
          if(detail_index == (VEHICLE_RESOURCE_DETAIL_COUNT - 1))
            {
              tag_index = VEHICLE_RESOURCE_HIGH_ANIMATION_TAG_FIRST + animation_group;
            }
          else
            {
              tag_index = VEHICLE_RESOURCE_ANIMATION_TAG_FIRST + animation_group;
            }
          record_->animated_shape_indices[(detail_index * VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT) +
                                          animation_group] =
            find_resource_shape_tag_index(record_->object_sources[source_index],
                                          g_VEHICLE_SHAPE_TAG_POINTERS[tag_index]);

          for(frame_index = VEHICLE_RESOURCE_ANIMATION_FRAME_FIRST;
              frame_index <= VEHICLE_RESOURCE_ANIMATION_FRAME_COUNT;
              frame_index += 1)
            {
              tag_index = VEHICLE_RESOURCE_ANIMATION_FRAME_TAG_FIRST +
                          (animation_group * VEHICLE_RESOURCE_ANIMATION_FRAME_COUNT) +
                          (frame_index - VEHICLE_RESOURCE_ANIMATION_FRAME_FIRST);
              shape =
                (uint8 *) locate_shape(record_->shape_collections[source_index],
                                       (const uint32 *) g_VEHICLE_SHAPE_TAG_POINTERS[tag_index]);
              record_->animated_shapes[(detail_index * VEHICLE_RESOURCE_ANIMATION_DETAIL_STRIDE) +
                                       (animation_group * VEHICLE_RESOURCE_ANIMATION_GROUP_STRIDE) +
                                       frame_index] = shape;
              if(shape != NULL)
                normalize_render_shape_metadata(shape);
            }

          shape_index =
            record_
              ->animated_shape_indices[(detail_index * VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT) +
                                       animation_group];
          if(shape_index >= 0)
            {
              record_
                ->animated_shapes[(detail_index * VEHICLE_RESOURCE_ANIMATION_DETAIL_STRIDE) +
                                  (animation_group * VEHICLE_RESOURCE_ANIMATION_GROUP_STRIDE)] =
                record_->shape_pointer_allocations[source_index][shape_index];
            }
        }

      shape_pointers = record_->shape_pointer_allocations[source_index];
      record_->animated_shapes[(detail_index * VEHICLE_RESOURCE_ANIMATION_DETAIL_STRIDE) +
                               VEHICLE_RESOURCE_ANIMATION_PRIMARY_OVERRIDE] =
        shape_pointers[record_->animated_shape_indices[detail_index *
                                                       VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT]];
      record_->animated_shapes[(detail_index * VEHICLE_RESOURCE_ANIMATION_DETAIL_STRIDE) +
                               VEHICLE_RESOURCE_ANIMATION_SECONDARY_OVERRIDE] =
        shape_pointers[record_->animated_shape_indices
                         [(detail_index * VEHICLE_RESOURCE_ANIMATION_GROUP_COUNT) + 1]];
    }

  for(detail_index = 0; detail_index < VEHICLE_RESOURCE_BRAKE_DETAIL_COUNT; detail_index += 1)
    {
      source_index = (VEHICLE_RESOURCE_DETAIL_COUNT - 1) - detail_index;
      for(shape_index = 0; shape_index < VEHICLE_RESOURCE_BRAKE_TAG_COUNT; shape_index += 1)
        {
          record_->brake_light_shape_indices[(detail_index * VEHICLE_RESOURCE_BRAKE_TAG_COUNT) +
                                             shape_index] =
            find_resource_shape_tag_index(
              record_->object_sources[source_index],
              g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_RESOURCE_BRAKE_TAG_FIRST + shape_index]);
        }

      shape = (uint8 *) locate_shape(
        record_->shape_collections[source_index],
        (const uint32 *) g_VEHICLE_SHAPE_TAG_POINTERS[VEHICLE_RESOURCE_BRAKE_VARIANT_TAG]);
      record_->brake_light_shapes[(detail_index * VEHICLE_RESOURCE_BRAKE_TAG_COUNT) + 1] = shape;
      if(shape != NULL)
        normalize_render_shape_metadata(shape);

      shape_index =
        record_->brake_light_shape_indices[detail_index * VEHICLE_RESOURCE_BRAKE_TAG_COUNT];
      if(shape_index >= 0)
        {
          record_->brake_light_shapes[detail_index * VEHICLE_RESOURCE_BRAKE_TAG_COUNT] =
            record_->shape_pointer_allocations[source_index][shape_index];
        }
    }
}

void
load_vehicle_render_resource_slot(const char *identifier_, int32 slot_)
{
  RenderResourceRecord *record;
  char                  path[VEHICLE_RESOURCE_PATH_BYTES];
  int32                 object_count;

  copy_string(path, VEHICLE_RESOURCE_WRAPPER_PATH_PREFIX);
  append_string(path, identifier_);
  append_string(path, VEHICLE_RESOURCE_WRAPPER_PATH_SUFFIX);

  record               = &g_CAR_RENDER_RESOURCE_RECORDS[slot_];
  record->wrapper_file = load_headered_resource_file(path, NULL);
  initialize_vehicle_render_resource_record(record->wrapper_file, record);
  if(slot_ == VEHICLE_RESOURCE_SLOT_PLAYER)
    initialize_player_vehicle_wheel_shapes(record);

  object_count      = record->objects[RENDER_SHAPE_POINTER_COUNT - 1]->shape_count;
  record->sort_keys = (uint32 *) allocate_named_memory(
    VEHICLE_RESOURCE_SORT_ORDER_NAME,
    (uint32) (object_count + VEHICLE_RESOURCE_SORT_ORDER_EXTRA_VALUES) * sizeof(int32),
    VEHICLE_RESOURCE_MEMORY_FLAGS);
  fill_fixed16_ramp(object_count, (int32 *) record->sort_keys);
  record->sort_refresh_countdown                                    = 1;
  record->reserved_030[VEHICLE_RESOURCE_DYNAMIC_INITIALIZED_OFFSET] = 0;

  if(g_VEHICLE_ROAD_ANIMATION_STATE.dynamic_data_enabled == 0)
    {
      g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_] =
        (VehicleDynamicRenderObject *) get_headered_pointer_array_element(
          record->wrapper_file,
          VEHICLE_RESOURCE_DYNAMIC_OBJECT_INDEX);
      g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_]->links[0] = 0;
      g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_]->links[1] = 0;
      g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_]->links[VEHICLE_RESOURCE_OBJECT_LINK_WORD_COUNT - 1] =
        0;
      return;
    }

  copy_string(path, VEHICLE_RESOURCE_WRAPPER_PATH_PREFIX);
  append_string(path, identifier_);
  append_string(path, VEHICLE_RESOURCE_DYNAMIC_PATH_SUFFIX);
  g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_] =
    (VehicleDynamicRenderObject *) allocate_named_memory(VEHICLE_RESOURCE_DYNAMIC_NAME,
                                                         VEHICLE_RESOURCE_DYNAMIC_DATA_BYTES,
                                                         VEHICLE_RESOURCE_MEMORY_FLAGS);
  load_vehicle_dynamic_resource(g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_], path);
  if(initialize_camera_io() != 0)
    {
      memcpy(path,
             VEHICLE_RESOURCE_CACHE_SEPARATOR,
             sizeof(VEHICLE_RESOURCE_CACHE_SEPARATOR));
      append_string(path, VEHICLE_RESOURCE_WRAPPER_PATH_PREFIX);
      append_string(path, identifier_);
      append_string(path, VEHICLE_RESOURCE_DYNAMIC_FILE_SUFFIX);
      (void) WriteMacFile(path,
                          g_VEHICLE_DYNAMIC_RENDER_OBJECTS[slot_],
                          VEHICLE_RESOURCE_DYNAMIC_DATA_BYTES);
    }
}

void
load_vehicle_render_resources(int32 vehicle_index_, int32 opponent_index_)
{
  uint32 hardware_seed[RANDOM_STATE_WORD_COUNT];
  int32  selected[VEHICLE_RESOURCE_SELECTION_COUNT];
  int32  identifier_index;
  int32  random_index;
  int32  seed_index;
  int32  selection_index;

  memset(selected, 0, sizeof(selected));
  for(seed_index = 0; seed_index < RANDOM_STATE_WORD_COUNT; seed_index += 1)
    hardware_seed[seed_index] = ReadHardwareRandomNumber();
  memcpy(g_RANDOM_STATE, hardware_seed, sizeof(hardware_seed));

  load_vehicle_render_resource_slot(g_VEHICLE_IDENTIFIER_POINTERS[vehicle_index_],
                                    VEHICLE_RESOURCE_SLOT_PLAYER);
  if(g_OPPONENT_VEHICLE_INDEX == VEHICLE_SIMULATION_MODE_DEFAULT_OPPONENT)
    identifier_index = VEHICLE_IDENTIFIER_DEFAULT;
  else if(G_OPPONENT_VEHICLE_OVERRIDE == 0)
    identifier_index = opponent_index_;
  else
    identifier_index = VEHICLE_IDENTIFIER_OVERRIDE;
  load_vehicle_render_resource_slot(g_VEHICLE_IDENTIFIER_POINTERS[identifier_index],
                                    VEHICLE_RESOURCE_SLOT_OPPONENT);
  load_vehicle_render_resource_slot(g_VEHICLE_IDENTIFIER_POINTERS[VEHICLE_IDENTIFIER_POLICE],
                                    VEHICLE_RESOURCE_SLOT_POLICE);

  for(selection_index = 0; selection_index < VEHICLE_RESOURCE_TRAFFIC_SLOT_COUNT;
      selection_index += 1)
    {
      do
        {
          random_index = scale_by_random_fraction(VEHICLE_RESOURCE_TRAFFIC_IDENTIFIER_COUNT);
        }
      while(selected[random_index] != 0);

      selected[random_index] = 1;
      load_vehicle_render_resource_slot(
        g_VEHICLE_IDENTIFIER_POINTERS[VEHICLE_RESOURCE_TRAFFIC_IDENTIFIER_FIRST + random_index],
        VEHICLE_RESOURCE_SLOT_TRAFFIC_FIRST + selection_index);
    }
}
