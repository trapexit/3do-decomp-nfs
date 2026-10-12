#include "pointer_array.h"

#define POINTER_ARRAY_HEADER_ELEMENTS 2

void *
get_pointer_array_element(void *const elements_[], int32 index_)
{
  return elements_[index_];
}

void *
get_headered_pointer_array_element(void *const elements_[], int32 index_)
{
  return elements_[index_ + POINTER_ARRAY_HEADER_ELEMENTS];
}

void *
get_nested_pointer_array_element(void *const arrays_[], int32 outer_index_, int32 inner_index_)
{
  void *const *elements;

  elements = (void *const *) arrays_[outer_index_];
  return elements[inner_index_];
}

void *
get_headered_nested_pointer_array_element(void *const arrays_[],
                                          int32       outer_index_,
                                          int32       inner_index_)
{
  void *const *elements;

  elements = (void *const *) arrays_[outer_index_ + POINTER_ARRAY_HEADER_ELEMENTS];
  return elements[inner_index_];
}

void *
get_three_level_pointer_array_element(void *const arrays_[],
                                      int32       first_index_,
                                      int32       second_index_,
                                      int32       third_index_)
{
  void *const *second_level;
  void *const *third_level;

  second_level = (void *const *) arrays_[first_index_];
  third_level  = (void *const *) second_level[second_index_];
  return third_level[third_index_];
}

void *
get_headered_three_level_pointer_array_element(void *const arrays_[],
                                               int32       first_index_,
                                               int32       second_index_,
                                               int32       third_index_)
{
  void *const *second_level;
  void *const *third_level;

  second_level = (void *const *) arrays_[first_index_ + POINTER_ARRAY_HEADER_ELEMENTS];
  third_level  = (void *const *) second_level[second_index_];
  return third_level[third_index_];
}

void *
get_headered_four_level_pointer_array_element(void *const arrays_[],
                                              int32       first_index_,
                                              int32       second_index_,
                                              int32       third_index_,
                                              int32       fourth_index_)
{
  void *const *second_level;
  void *const *third_level;
  void *const *fourth_level;

  second_level = (void *const *) arrays_[first_index_ + POINTER_ARRAY_HEADER_ELEMENTS];
  third_level  = (void *const *) second_level[second_index_];
  fourth_level = (void *const *) third_level[third_index_];
  return fourth_level[fourth_index_];
}
