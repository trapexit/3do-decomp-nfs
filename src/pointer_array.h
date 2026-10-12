#pragma once

#include "types.h"

void *
get_pointer_array_element(void *const elements_[], int32 index_);
void *
get_headered_pointer_array_element(void *const elements_[], int32 index_);
void *
get_nested_pointer_array_element(void *const arrays_[], int32 outer_index_, int32 inner_index_);
void *
get_three_level_pointer_array_element(void *const arrays_[],
                                      int32       first_index_,
                                      int32       second_index_,
                                      int32       third_index_);
void *
get_headered_nested_pointer_array_element(void *const arrays_[],
                                          int32       outer_index_,
                                          int32       inner_index_);
void *
get_headered_three_level_pointer_array_element(void *const arrays_[],
                                               int32       first_index_,
                                               int32       second_index_,
                                               int32       third_index_);
void *
get_headered_four_level_pointer_array_element(void *const arrays_[],
                                              int32       first_index_,
                                              int32       second_index_,
                                              int32       third_index_,
                                              int32       fourth_index_);
