#pragma once

#include "base_types.h"
#include "geometry.h"

struct SimulationSource;

#define TRAFFIC_VEHICLE_SOUND_TABLE_COUNT 10

int32
is_sound_effect_playable(int32 sound_id_);
int32
select_sound_instrument_index(int32 sound_id_, int32 sound_class_, int32 variant_);
int32
calculate_sound_pan(int32 angle_);
int32
calculate_sound_distance_gain(int32 distance_);
void
calculate_spatial_sound_parameters(const Vector3 *position_, int32 *pan_, int32 *distance_);
int32
select_spatial_sound_sample(int32 gain_, int32 sound_class_, int32 sound_variant_);
int32
calculate_spatial_sound_frequency(int32 sound_id_, int32 instrument_index_, int32 gain_);
void
stop_spatial_sound_effect(int32 sound_id_);
int32
update_dolby_spatial_sound(int32 sound_id_,
                           int32 instrument_index_,
                           int32 sample_priority_,
                           int32 amplitude_,
                           int32 pan_);
void
update_audio_voice_spatial_sound(int32 sound_id_,
                                 int32 instrument_index_,
                                 int32 frequency_,
                                 int32 amplitude_,
                                 int32 pan_);
void
play_anonymous_spatial_sound(int32 sample_, int32 frequency_, int32 amplitude_, int32 pan_);
void
update_vehicle_proximity_alert(void);
void
update_player_vehicle_runtime_audio(struct SimulationSource *source_, int32 pan_, int32 distance_);
void
update_active_vehicle_spatial_audio_channels(void);
void
request_traffic_vehicle_spatial_sound(int32 source_index_, int32 variant_);
void
refresh_vehicle_runtime_audio(void);
int32
select_traffic_vehicle_sound_instrument(void);
int32
select_traffic_vehicle_sound_amplitude(void);
void
play_spatial_sound_effect(int32 sound_id_,
                          int32 sound_class_,
                          int32 sound_variant_,
                          int32 gain_,
                          int32 distance_,
                          int32 pan_);
void
play_spatial_sound_effect_unscaled(int32 sound_id_,
                                   int32 sound_class_,
                                   int32 sound_variant_,
                                   int32 gain_,
                                   int32 distance_,
                                   int32 pan_);
void
play_spatial_sound_effect_scaled(int32 sound_id_,
                                 int32 sound_class_,
                                 int32 sound_variant_,
                                 int32 gain_,
                                 int32 distance_,
                                 int32 pan_);
void
play_spatial_sound_effect_attenuated(int32 sound_id_,
                                     int32 sound_class_,
                                     int32 sound_variant_,
                                     int32 gain_,
                                     int32 distance_,
                                     int32 pan_);
void
play_vehicle_spatial_sound_attenuated(int32 sound_id_,
                                      int32 sound_class_,
                                      int32 sound_variant_,
                                      int32 gain_,
                                      int32 distance_,
                                      int32 pan_);

extern uint8       g_VEHICLE_PROXIMITY_ALERT_ACTIVE;
extern int32       g_VEHICLE_PROXIMITY_ALERT_COUNTDOWN;
extern int32       g_VEHICLE_AUDIO_GAIN_TABLE[256];
extern int32       g_SPATIAL_SOUND_FREQUENCY_CACHE[21];
extern int32       g_VEHICLE_AUDIO_PAN_INDEX;
extern int32       g_VEHICLE_SURFACE_AUDIO_FREQUENCY;
extern uint8       g_VEHICLE_SURFACE_AUDIO_FREQUENCY_INCREASING;
extern int32       g_TRAFFIC_VEHICLE_SOUND_MINIMUM_FRAMES;
extern int32       g_TRAFFIC_VEHICLE_SOUND_RANDOM_FRAMES;
extern int32       g_TRAFFIC_VEHICLE_SOUND_VARIANT;
extern int32       g_TRAFFIC_VEHICLE_SOUND_COUNTDOWN;
extern int32       g_TRAFFIC_VEHICLE_SOUND_SOURCE_INDEX;
extern const int32 G_TRAFFIC_VEHICLE_SOUND_INSTRUMENTS[TRAFFIC_VEHICLE_SOUND_TABLE_COUNT];
extern const int32 G_TRAFFIC_VEHICLE_SOUND_AMPLITUDES[TRAFFIC_VEHICLE_SOUND_TABLE_COUNT];
