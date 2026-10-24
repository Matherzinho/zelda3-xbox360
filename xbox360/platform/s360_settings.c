// Settings file: <name>.ini beside the .xex (relative paths start at game:\), one "key = choice" a line, where the
// choice is the index in the setting's list. Unknown keys and out-of-range values are ignored.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "s360_internal.h"

static int NumChoices(const S360Setting *s) {
  int n = 0;
  while (s->choices[n]) n++;
  return n;
}

static void Apply(S360Setting *list, int n, const char *key, int value) {
  for (int i = 0; i < n; i++) {
    if (strcmp(list[i].key, key) == 0) {
      if (value >= 0 && value < NumChoices(&list[i])) list[i].value = value;
      return;
    }
  }
}

static void FileName(const S360Game *game, char *buf, size_t n) { snprintf(buf, n, "%s.ini", game->name); }

void s360_settings_load(const S360Game *game, S360Setting *platform, int num_platform) {
  for (int i = 0; i < num_platform; i++) platform[i].value = platform[i].def;
  for (int i = 0; i < game->num_settings; i++) game->settings[i].value = game->settings[i].def;
  char name[64], line[160];
  FileName(game, name, sizeof name);
  FILE *f = fopen(name, "r");
  if (!f) {
    s360_log("s360: no %s yet, using defaults", name);
    return;
  }
  while (fgets(line, sizeof line, f)) {
    char key[64];
    int value;
    if (line[0] == '#' || sscanf(line, " %63[^= ] = %d", key, &value) != 2) continue;
    Apply(platform, num_platform, key, value);
    Apply(game->settings, game->num_settings, key, value);
  }
  fclose(f);
  s360_log("s360: settings loaded from %s", name);
}

static void WriteList(FILE *f, const S360Setting *list, int n) {
  for (int i = 0; i < n; i++)
    fprintf(f, "%s = %d   # %s: %s\n", list[i].key, list[i].value, list[i].label, list[i].choices[list[i].value]);
}

void s360_settings_save(const S360Game *game, const S360Setting *platform, int num_platform) {
  char name[64];
  FileName(game, name, sizeof name);
  FILE *f = fopen(name, "w");
  if (!f) {
    s360_log("s360: unable to write %s", name);
    return;
  }
  fprintf(f, "# %s settings, written by the in-game menu (right stick click). Values are choice numbers from 0.\n",
          game->title);
  WriteList(f, platform, num_platform);
  WriteList(f, game->settings, game->num_settings);
  fclose(f);
}
