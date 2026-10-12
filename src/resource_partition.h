#pragma once

#include "types.h"

int32
find_resource_partition_id(const void *resource_, const char *name_);
int32
find_resource_object_tag_index(const void *resource_, const char *name_);
int32
find_resource_shape_tag_index(const void *resource_, const char *name_);
void
get_resource_partition(const void *resource_,
                       int32       partition_id_,
                       int32      *element_count_,
                       int32     **elements_);
int32 *
get_resource_partition_data(const void *resource_, int32 partition_id_, int32 *element_count_);
