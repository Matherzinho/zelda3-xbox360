#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "assets.h"
#include "config.h"
#include "features.h"
#include "variables.h"
#include "zelda_rtl.h"
#include "audio.h"
#include "snes/ppu.h"
#include "util.h"

const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];
static uint8 *g_asset_file;

static bool AssetError(char *error, size_t size, const char *message) {
  if (error && size) snprintf(error, size, "%s", message);
  return false;
}

static uint32 ReadLe32(const uint8 *data) {
  return (uint32)data[0] | (uint32)data[1] << 8 | (uint32)data[2] << 16 | (uint32)data[3] << 24;
}

bool ZeldaGameLoadAssets(const char *path, char *error, size_t error_size) {
  size_t length = 0;
  const char *filename = path && *path ? path : "zelda3_assets.dat";
  uint8 *data = ReadWholeFile(filename, &length);
  if (!data && strcmp(filename, "zelda3_assets.dat") == 0) {
    size_t bps_length = 0, rom_length = 0;
    uint8 *bps = ReadWholeFile("zelda3_assets.bps", &bps_length);
    if (!bps)
      return AssetError(error, error_size, "Cannot open zelda3_assets.dat or zelda3_assets.bps");
    uint8 *rom = ReadWholeFile("zelda3.sfc", &rom_length);
    if (!rom) {
      free(bps);
      return AssetError(error, error_size, "Missing file: zelda3.sfc");
    }
    data = ApplyBps(rom, rom_length, bps, bps_length, &length);
    free(rom);
    free(bps);
    if (!data)
      return AssetError(error, error_size, "Unable to apply zelda3_assets.bps; verify the ROM version");
  }
  if (!data)
    return AssetError(error, error_size, "Cannot open zelda3_assets.dat (check ZELDA3_ASSETS path)");

  static const char sig[] = { kAssets_Sig };
  if (length < 88 + kNumberOfAssets * 4 || memcmp(data, sig, 48) || ReadLe32(data + 80) != kNumberOfAssets) {
    free(data);
    return AssetError(error, error_size, "zelda3_assets.dat has an invalid header/version");
  }
  uint32 offset = 88 + kNumberOfAssets * 4 + ReadLe32(data + 84);
  for (size_t i = 0; i < kNumberOfAssets; i++) {
    uint32 size = ReadLe32(data + 88 + i * 4);
    offset = (offset + 3) & ~3;
    if ((uint64)offset + size > length) {
      free(data);
      return AssetError(error, error_size, "zelda3_assets.dat is truncated or corrupt");
    }
    g_asset_sizes[i] = size;
    g_asset_ptrs[i] = data + offset;
    offset += size;
  }
  free(g_asset_file);
  g_asset_file = data;
  if (g_config.features0 & kFeatures0_DimFlashes) {
    kPalette_DungBgMain[0x484] = 0x70;
    kPalette_DungBgMain[0x485] = 0x95;
    kPalette_DungBgMain[0x486] = 0x57;
  }
  if (error && error_size) error[0] = 0;
  return true;
}

void ZeldaGameInitialize(uint8 extra_left_right, uint8 msu_mode, const char *language) {
  ZeldaInitialize();
  g_zenv.ppu->extraLeftRight = UintMin(extra_left_right, kPpuExtraLeftRight);
  ZeldaEnableMsu(msu_mode);
  ZeldaSetLanguage(language);  // NULL: the language the assets were built from
}
void ZeldaGameSetWantedFeatures(uint32 wanted_features) {
  g_wanted_zelda_features = wanted_features;
}
bool ZeldaGameUpdate(uint16 input_state) { return ZeldaRunFrame(input_state); }
void ZeldaGameRender(uint8 *pixels, size_t pitch, uint32 render_flags) { ZeldaDrawPpuFrame(pixels, pitch, render_flags); }
MemBlk FindInAssetArray(int asset, int idx) {
  return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}
