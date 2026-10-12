#pragma once

#include "geometry.h"

extern int32 g_MATRIX_COLLISION_ALTERNATE_MODE;
extern int32 g_MATRIX_COLLISION_IMPULSE_SCALE;
extern int32 g_MATRIX_COLLISION_RUNTIME_RESERVED_TAIL;

int32
find_matrix_collision_contact(MatrixControllerState *first_state_,
                              MatrixControllerState *second_state_,
                              Vector3               *contact_point_,
                              Vector3               *collision_normal_,
                              const int32           *second_extent_projection_,
                              const int32           *first_extent_projection_,
                              const int32           *center_projection_);
int32
resolve_matrix_controller_collision(MatrixControllerState *first_state_,
                                    MatrixControllerState *second_state_,
                                    Vector3               *first_contact_,
                                    Vector3               *second_contact_);
int32
resolve_alternate_matrix_collision(MatrixControllerState *first_state_,
                                   MatrixControllerState *second_state_,
                                   Vector3               *first_contact_,
                                   Vector3               *second_contact_);
int32
resolve_standard_matrix_collision(MatrixControllerState *first_state_,
                                  MatrixControllerState *second_state_,
                                  Vector3               *first_contact_,
                                  Vector3               *second_contact_);
int32
apply_matrix_collision_impulse(MatrixControllerState *first_state_,
                               MatrixControllerState *second_state_,
                               const Vector3         *contact_point_,
                               const Vector3         *collision_normal_);
void
resolve_matrix_controller_surface_collision(MatrixControllerState *controller_,
                                            const Vector3         *normal_,
                                            const Vector3         *surface_point_);
void
apply_matrix_surface_collision_response(MatrixControllerState *controller_,
                                        const Vector3         *contact_point_,
                                        const Vector3         *contact_velocity_,
                                        const Vector3         *normal_);
