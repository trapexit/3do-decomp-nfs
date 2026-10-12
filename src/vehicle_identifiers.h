#pragma once

#define VEHICLE_IDENTIFIER_COUNT 25

typedef struct VehicleIdentifiers
{
  char toyota_supra[8];
  char lamborghini_diablo[8];
  char porsche_911[8];
  char corvette_zr1[8];
  char ferrari_512tr[8];
  char dodge_viper[8];
  char acura_nsx[8];
  char mazda_rx7[8];
  char scooter[8];
  char police_mustang[8];
  char police_camaro[12];
  char police_caprice[12];
  char police_victoria[8];
  char wagon[8];
  char honda_crx[4];
  char vandura[8];
  char bmw[4];
  char pontiac_sunbird[8];
  char volkswagen_jetta[8];
  char isuzu_rodeo[8];
  char nissan_axxess[8];
  char ford_probe[8];
  char jeep[8];
  char pontiac_lemans[8];
  char pickup[8];
} VehicleIdentifiers;

extern const VehicleIdentifiers G_VEHICLE_IDENTIFIERS;
extern const char              *g_VEHICLE_IDENTIFIER_POINTERS[VEHICLE_IDENTIFIER_COUNT];
