#include <stdint.h>
#include <string.h>
#include <sys/statvfs.h>

#include "../slstatus.h"
#include "../util.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#define WIDGETS_CPU_FREQ "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"

#define WIDGETS_POWER_SUPPLY_CAPACITY "/sys/class/power_supply/%s/capacity"
#define WIDGETS_POWER_SUPPLY_STATUS "/sys/class/power_supply/%s/status"
#define WIDGETS_WIDGETS_POWER_SUPPLY_CHARGE                                    \
  "/sys/class/power_supply/%s/charge_now"
#define WIDGETS_POWER_SUPPLY_ENERGY "/sys/class/power_supply/%s/energy_now"
#define WIDGETS_POWER_SUPPLY_CURRENT "/sys/class/power_supply/%s/current_now"
#define WIDGETS_POWER_SUPPLY_POWER "/sys/class/power_supply/%s/power_now"

#define WIDGETS_POWER_CHARGE_FULL "/sys/class/power_supply/%s/charge_full"
#define WIDGETS_POWER_CHARGE_NOW "/sys/class/power_supply/%s/charge_now"

/* HELPERS */

/* CPU */
/* in GHz */
double widgets_helper_cpu_freq_num() {
  uintmax_t freq;

  /* in kHz */
  if (pscanf(WIDGETS_CPU_FREQ, "%ju", &freq) != 1)
    return (double)0;

  return (double)freq / 1000000;
}

int widgets_helper_cpu_perc_num() {
  static long double a[7];
  long double b[7], sum;

  memcpy(b, a, sizeof(b));

  if (pscanf("/proc/stat", "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf", &a[0], &a[1],
             &a[2], &a[3], &a[4], &a[5], &a[6]) != 7)
    return 0;

  if (b[0] == 0)
    return 0;

  sum = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6]) -
        (a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6]);

  if (sum == 0)
    return 0;

  return (int)(100 *
               ((b[0] + b[1] + b[2] + b[5] + b[6]) -
                (a[0] + a[1] + a[2] + a[5] + a[6])) /
               sum);
}

int widgets_helper_cpu_temp() {
  const char *path = "/sys/class/thermal/thermal_zone2/temp";
  uintmax_t temp;
  if (pscanf(path, "%ju", &temp) != 1)
    return 0;
  return temp / 1000;
}

/* RAM */

double widgets_helper_ram_used() {
  uintmax_t total, dummy, free, buffers, cached, used, shmem;

  if (pscanf("/proc/meminfo",
             "MemTotal: %ju kB\n"
             "MemFree: %ju kB\n"
             "MemAvailable: %ju kB\n"
             "Buffers: %ju kB\n"
             "Cached: %ju kB\n"
             "SwapCached: %ju kB\n"
             "Active: %ju kB\n"
             "Inactive: %ju kB\n"
             "Active(anon): %ju kB\n"
             "Inactive(anon): %ju kB\n"
             "Active(file): %ju kB\n"
             "Inactive(file): %ju kB\n"
             "Unevictable: %ju kB\n"
             "Mlocked: %ju kB\n"
             "SwapTotal: %ju kB\n"
             "SwapFree: %ju kB\n"
             "Zswap: %ju kB\n"
             "Zswapped: %ju kB\n"
             "Dirty: %ju kB\n"
             "Writeback: %ju kB\n"
             "AnonPages: %ju kB\n"
             "Mapped: %ju kB\n"
             "Shmem: %ju kB\n",
             &total, &free, &dummy, &buffers, &cached, &dummy, &dummy, &dummy,
             &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
             &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &shmem) != 23)
    return 0;

  used = (total - free - buffers - cached) + shmem;
  return (double)used / 1024 / 1024;
}

/* DISK */

double widgets_helper_disk_free(const char *path) {

  struct statvfs fs;

  if (statvfs(path, &fs) < 0) {
    warn("statvfs '%s':", path);
    return 0;
  }

  return (double)(fs.f_frsize * fs.f_bavail) / 1024 / 1024 / 1024;
}

/* BATTERY */

int widgets_helper_battery_charge_full(const char *bat) {
  int battery_cap;
  char path[PATH_MAX];

  if (esnprintf(path, sizeof(path), WIDGETS_POWER_CHARGE_FULL, bat) < 0)
    return 0;

  if (pscanf(path, "%d", &battery_cap) != 1)
    return 0;

  return battery_cap;
}

int widgets_helper_battery_charge_now(const char *bat) {
  int battery_now;
  char path[PATH_MAX];

  if (esnprintf(path, sizeof(path), WIDGETS_POWER_CHARGE_NOW, bat) < 0)
    return 0;

  if (pscanf(path, "%d", &battery_now) != 1)
    return 0;

  return battery_now;
}

int widgets_helper_battery_perc(const char *bat) {
  int charge_now = widgets_helper_battery_charge_now(bat);
  int charge_full = widgets_helper_battery_charge_full(bat);

  return (charge_now * 100) / charge_full;
}

int widget_helper_battery_normalized_level(int level) {
  if (level < 5)
    return 0;
  if (level < 15)
    return 10;
  if (level < 25)
    return 20;
  if (level < 35)
    return 30;
  if (level < 45)
    return 40;
  if (level < 55)
    return 50;
  if (level < 65)
    return 60;
  if (level < 75)
    return 70;
  if (level < 85)
    return 80;
  if (level < 95)
    return 90;
  return 100;
}

enum BATTERY_STATE { UNKNOWN, NOT_CHARGING, CHARGING, DISCHARGING, CHARGED };

enum BATTERY_STATE widgets_helper_battery_state(const char *bat) {
  static struct {
    char *state;
    enum BATTERY_STATE symbol;
  } map[] = {
      {"Charging", CHARGING},
      {"Discharging", DISCHARGING},
      {"Full", CHARGED},
      {"Not charging", NOT_CHARGING},
  };
  size_t i;
  char path[PATH_MAX], state[12];

  if (esnprintf(path, sizeof(path), WIDGETS_POWER_SUPPLY_STATUS, bat) < 0)
    return UNKNOWN;
  if (pscanf(path, "%12[a-zA-Z ]", state) != 1)
    return UNKNOWN;

  for (i = 0; i < LEN(map); i++)
    if (!strcmp(map[i].state, state))
      break;

  return (i == LEN(map)) ? UNKNOWN : map[i].symbol;
}

const char *widgets_helper_battery_icon(enum BATTERY_STATE state, int level) {
  switch (level) {
  case 0:
    return (state == CHARGING) ? "󰢟" : "󰂎";
  case 10:
    return (state == CHARGING) ? "󰢜" : "󰁺";
  case 20:
    return (state == CHARGING) ? "󰂆" : "󰁻";
  case 30:
    return (state == CHARGING) ? "󰂇" : "󰁼";
  case 40:
    return (state == CHARGING) ? "󰂈" : "󰁽";
  case 50:
    return (state == CHARGING) ? "󰢝" : "󰁾";
  case 60:
    return (state == CHARGING) ? "󰂉" : "󰁿";
  case 70:
    return (state == CHARGING) ? "󰢞" : "󰂀";
  case 80:
    return (state == CHARGING) ? "󰂊" : "󰂁";
  case 90:
    return (state == CHARGING) ? "󰂋" : "󰂂";
  case 100:
    return (state == CHARGING) ? "󰂅" : "󰁹";
  }

  return "󰂎";
}

/* WIDGETS */

const char *widgets_cpu(const char *unused) {
  double freq = widgets_helper_cpu_freq_num();
  int perc = widgets_helper_cpu_perc_num();
  int temp = widgets_helper_cpu_temp();

  return bprintf("%2d%% %.1fGHz %2d°", perc, freq, temp);
}

const char *widgets_ram(const char *unused) {
  return bprintf("%.1fGb", widgets_helper_ram_used());
}

const char *widgets_disk(const char *path) {
  return bprintf("%3.1fGb", widgets_helper_disk_free(path));
}

const char *widgets_battery(const char *bat) {
  int perc = widgets_helper_battery_perc(bat);
  int level = widget_helper_battery_normalized_level(perc);
  enum BATTERY_STATE state = widgets_helper_battery_state(bat);
  return bprintf("%s %d%%", widgets_helper_battery_icon(state, level), perc);
}
