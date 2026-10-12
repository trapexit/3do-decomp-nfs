#include "vehicle_identifiers.h"

#define COMPILE_TIME_SIZE_ASSERT(name, expression)                                                 \
  typedef char compile_time_size_assert_##name[(expression) ? 1 : -1]

const VehicleIdentifiers G_VEHICLE_IDENTIFIERS = {
  "TSupra",  "LDiablo",   "P911",       "CZR1",   "F512TR", "DVIPER", "ANSX",    "MRX7", "Scooter",
  "CopMust", "CopCamaro", "CopCaprice", "CopVic", "Wagon",  "CRX",    "Vandura", "BMW",  "SunBird",
  "JETTA",   "RODEO",     "axxess",     "probe",  "Jeep",   "Lemans", "Pickup"
};
const char *g_VEHICLE_IDENTIFIER_POINTERS[VEHICLE_IDENTIFIER_COUNT] = {
  G_VEHICLE_IDENTIFIERS.toyota_supra,
  G_VEHICLE_IDENTIFIERS.lamborghini_diablo,
  G_VEHICLE_IDENTIFIERS.porsche_911,
  G_VEHICLE_IDENTIFIERS.corvette_zr1,
  G_VEHICLE_IDENTIFIERS.ferrari_512tr,
  G_VEHICLE_IDENTIFIERS.dodge_viper,
  G_VEHICLE_IDENTIFIERS.acura_nsx,
  G_VEHICLE_IDENTIFIERS.mazda_rx7,
  G_VEHICLE_IDENTIFIERS.scooter,
  G_VEHICLE_IDENTIFIERS.police_mustang,
  G_VEHICLE_IDENTIFIERS.police_camaro,
  G_VEHICLE_IDENTIFIERS.police_caprice,
  G_VEHICLE_IDENTIFIERS.police_victoria,
  G_VEHICLE_IDENTIFIERS.wagon,
  G_VEHICLE_IDENTIFIERS.honda_crx,
  G_VEHICLE_IDENTIFIERS.vandura,
  G_VEHICLE_IDENTIFIERS.bmw,
  G_VEHICLE_IDENTIFIERS.pontiac_sunbird,
  G_VEHICLE_IDENTIFIERS.volkswagen_jetta,
  G_VEHICLE_IDENTIFIERS.isuzu_rodeo,
  G_VEHICLE_IDENTIFIERS.nissan_axxess,
  G_VEHICLE_IDENTIFIERS.ford_probe,
  G_VEHICLE_IDENTIFIERS.jeep,
  G_VEHICLE_IDENTIFIERS.pontiac_lemans,
  G_VEHICLE_IDENTIFIERS.pickup
};

COMPILE_TIME_SIZE_ASSERT(vehicle_identifiers_size, sizeof(G_VEHICLE_IDENTIFIERS) == 200);
COMPILE_TIME_SIZE_ASSERT(vehicle_identifier_pointer_count,
                         sizeof(g_VEHICLE_IDENTIFIER_POINTERS) == 100);
