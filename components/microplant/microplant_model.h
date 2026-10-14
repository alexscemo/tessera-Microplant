#pragma once
// The plant's data model: the four readings, their good ranges, what each reading's status is, the plant's mood, the
// demo data, and the small time helpers. No LVGL and no plugin API in here, so it can be tested on a computer.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome::microplant {

// ===================================================================================================================
//  Configuration. Everything a person sets per plant (the sensors and the good ranges) is a tile option in the
//  editor (tessera-plugin.yaml); the poll interval and demo mode are settings of the screen (plugin.yaml). What is
//  left here are the constants behind them.
// ===================================================================================================================
namespace config {
// How often a tile asks Home Assistant for the sensors of its options, until the setting "Plant poll seconds" says.
constexpr uint32_t POLL_SECONDS = 60;
constexpr uint32_t POLL_MIN_SECONDS = 30, POLL_MAX_SECONDS = 300;
// After an ask that failed or got no answer, the next one comes this much sooner than a whole interval.
constexpr uint32_t RETRY_SECONDS = 20;
// A tile without an answer for this long says it is offline (and keeps showing the last readings).
constexpr uint32_t OFFLINE_AFTER_SECONDS = 330;
// The ask covers the last minute, so the state at its start (include_start_time_state) is always in it, even when
// the screen's clock runs a little ahead of Home Assistant's.
constexpr uint32_t POLL_WINDOW_SECONDS = 60;

// How far outside its good range a reading may be before amber turns red. Amber: outside the range by less than this;
// red: by more. Light is a ratio (half the lowest, twice the highest), the rest are units of the reading.
constexpr float MOISTURE_MARGIN = 10.0f;    // percentage points
constexpr float TEMPERATURE_MARGIN = 3.0f;  // degrees Celsius
constexpr float HUMIDITY_MARGIN = 10.0f;    // percentage points
constexpr float LIGHT_FACTOR = 2.0f;        // times

// The mood after a watering: the plant says thank you this long.
constexpr uint32_t THANKS_SECONDS = 10;
// The history a reading's card shows: the hourly means of Home Assistant's statistics.
constexpr int HISTORY_HOURS = 24;
// A temperature attribute above this is in degrees Fahrenheit (see celsius_of_attribute).
constexpr float FAHRENHEIT_ABOVE = 45.0f;
// A plant sensor's battery at or below this (percent) shows a small battery mark on the plant: change it soon.
constexpr float BATTERY_LOW_PERCENT = 25.0f;

// Demo data: the soil starts full and drains one point every few seconds after a watering, so a person sees every mood
// within minutes; the air and the light wander on a cycle of ten minutes.
constexpr float DEMO_MOISTURE_FULL = 76.0f, DEMO_MOISTURE_FLOOR = 9.0f;
constexpr float DEMO_SECONDS_PER_POINT = 12.0f;
constexpr uint32_t DEMO_CYCLE_SECONDS = 600;
// A demo plant was watered this long before the screen started: thirsty, so the first tap shows the thank you.
constexpr uint32_t DEMO_FIRST_WATERED_AGO = 3 * 86400 + 4 * 3600;
}  // namespace config

// ---- The readings ----

enum Metric : uint8_t { MOISTURE, TEMPERATURE, HUMIDITY, LIGHT, METRIC_COUNT };

// Where a reading stands against its good range.
enum class Status : uint8_t { NONE, GOOD, LOW_WARN, LOW_BAD, HIGH_WARN, HIGH_BAD };

struct Range {
  float low, high;
};

// The default good ranges, also the defaults of the tile's options (tessera-plugin.yaml): a typical leafy houseplant.
constexpr Range DEFAULT_RANGES[METRIC_COUNT] = {{30, 70}, {16, 28}, {40, 70}, {1000, 25000}};

inline bool is_low(Status s) { return s == Status::LOW_WARN || s == Status::LOW_BAD; }
inline bool is_high(Status s) { return s == Status::HIGH_WARN || s == Status::HIGH_BAD; }
// 0 good (or no reading), 1 amber, 2 red.
inline int level(Status s) {
  return s == Status::LOW_BAD || s == Status::HIGH_BAD ? 2 : s == Status::LOW_WARN || s == Status::HIGH_WARN ? 1 : 0;
}

// The edges where amber turns red, below and above the range.
inline void red_edges(Metric metric, Range range, float &below, float &above) {
  if (metric == LIGHT) {
    below = range.low / config::LIGHT_FACTOR;
    above = range.high * config::LIGHT_FACTOR;
    return;
  }
  const float margin = metric == MOISTURE      ? config::MOISTURE_MARGIN
                       : metric == TEMPERATURE ? config::TEMPERATURE_MARGIN
                                               : config::HUMIDITY_MARGIN;
  below = range.low - margin;
  above = range.high + margin;
}

inline Status evaluate(Metric metric, float value, Range range) {
  if (std::isnan(value)) return Status::NONE;
  float below, above;
  red_edges(metric, range, below, above);
  if (value < below) return Status::LOW_BAD;
  if (value < range.low) return Status::LOW_WARN;
  if (value > above) return Status::HIGH_BAD;
  if (value > range.high) return Status::HIGH_WARN;
  return Status::GOOD;
}

// ---- The mood ----

enum class Mood : uint8_t {
  WAITING,   // no reading yet
  THRIVING,  // every reading in its range
  THANKS,    // just watered
  THIRSTY,   // soil a bit dry
  PARCHED,   // soil far too dry
  SOGGY,     // soil too wet
  TOO_HOT,   // not HOT: ESPHome defines HOT as a macro
  TOO_COLD,
  DARK,      // too little light
  SCORCHED,  // too much light
  DRY_AIR,
  HUMID,
};

// The mood of four statuses: the worst reading wins, the soil before the warmth, the light and the air when two are as
// bad as each other. A watering just now wins over everything for a few seconds.
inline Mood mood_of(const Status status[METRIC_COUNT], bool just_watered) {
  if (just_watered) return Mood::THANKS;
  bool any = false;
  for (int i = 0; i < METRIC_COUNT; ++i) any |= status[i] != Status::NONE;
  if (!any) return Mood::WAITING;
  static constexpr Metric ORDER[] = {MOISTURE, TEMPERATURE, LIGHT, HUMIDITY};
  for (int pass = 2; pass >= 1; --pass) {
    for (Metric m : ORDER) {
      const Status s = status[m];
      if (level(s) != pass) continue;
      switch (m) {
        case MOISTURE: return is_low(s) ? (pass == 2 ? Mood::PARCHED : Mood::THIRSTY) : Mood::SOGGY;
        case TEMPERATURE: return is_low(s) ? Mood::TOO_COLD : Mood::TOO_HOT;
        case LIGHT: return is_low(s) ? Mood::DARK : Mood::SCORCHED;
        default: return is_low(s) ? Mood::DRY_AIR : Mood::HUMID;
      }
    }
  }
  return Mood::THRIVING;
}

// A health score of 0 to 100: every amber reading takes 15, every red one 35.
inline int health_of(const Status status[METRIC_COUNT]) {
  int score = 100;
  for (int i = 0; i < METRIC_COUNT; ++i) score -= level(status[i]) == 2 ? 35 : level(status[i]) == 1 ? 15 : 0;
  return score < 0 ? 0 : score;
}

inline bool needs_water(Mood mood) { return mood == Mood::THIRSTY || mood == Mood::PARCHED; }

// ---- Demo data ----

// A reading of the demo plant at `now`, watered at `watered`: deterministic, so every screen shows the same story.
inline float demo_value(Metric metric, uint32_t now, uint32_t watered) {
  const float phase = 6.2831853f * static_cast<float>(now % config::DEMO_CYCLE_SECONDS) / config::DEMO_CYCLE_SECONDS;
  switch (metric) {
    case MOISTURE: {
      const float since = now > watered ? static_cast<float>(now - watered) : 0.0f;
      const float v = config::DEMO_MOISTURE_FULL - since / config::DEMO_SECONDS_PER_POINT;
      return v < config::DEMO_MOISTURE_FLOOR ? config::DEMO_MOISTURE_FLOOR : v;
    }
    case TEMPERATURE: {
      const float s = std::sin(phase);
      return 22.0f + 9.0f * s * s * s;  // mostly mild, a warm and a cold spell each cycle
    }
    case HUMIDITY: return 56.0f + 14.0f * std::sin(2.0f * phase + 1.0f);
    default: return 7000.0f + 6400.0f * std::sin(phase + 2.2f);
  }
}

// ---- Time ----

// Days since 1970 of a civil date (Howard Hinnant's days_from_civil).
inline int32_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  return era * 146097 + static_cast<int32_t>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}

// The civil date of days since 1970 (civil_from_days).
inline void civil_from_days(int32_t z, int &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = static_cast<int>(yoe) + era * 400 + (m <= 2);
}

// A moment as Home Assistant's websocket takes it: "2026-10-13T17:46:00+00:00". `out` holds at least 26 bytes.
inline void iso_utc(uint32_t epoch, char *out, size_t size) {
  int y;
  unsigned m, d;
  civil_from_days(static_cast<int32_t>(epoch / 86400), y, m, d);
  const uint32_t s = epoch % 86400;
  snprintf(out, size, "%04d-%02u-%02uT%02u:%02u:%02u+00:00", y, m, d, s / 3600, s / 60 % 60, s % 60);
}

// Seconds since 1970 of "2026-10-10 08:00:00", "2026-10-10T07:00:00.123+02:00" or "...Z". A time without a zone is
// the screen's local time: `local_offset` is local minus UTC in seconds. 0 for anything else.
inline uint32_t parse_moment(const char *text, int32_t local_offset) {
  if (!text) return 0;
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, used = 0;
  if (sscanf(text, "%4d-%2d-%2d%*1[ T]%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &s, &used) < 6) {
    // A date alone ("2026-10-10") is midnight of that day.
    if (sscanf(text, "%4d-%2d-%2d%n", &y, &mo, &d, &used) < 3) return 0;
    h = mi = s = 0;
  }
  if (y < 1971 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return 0;
  const char *rest = text + used;
  while (*rest == '.' || (*rest >= '0' && *rest <= '9')) ++rest;  // fractions of a second
  int64_t offset = local_offset;
  if (*rest == 'Z') {
    offset = 0;
  } else if (*rest == '+' || *rest == '-') {
    int oh = 0, om = 0;
    if (sscanf(rest + 1, "%2d:%2d", &oh, &om) >= 1) offset = (*rest == '-' ? -1 : 1) * (oh * 3600 + om * 60);
  }
  const int64_t local = static_cast<int64_t>(days_from_civil(y, mo, d)) * 86400 + h * 3600 + mi * 60 + s;
  const int64_t utc = local - offset;
  return utc > 0 && utc < 4102444800LL ? static_cast<uint32_t>(utc) : 0;
}

// A number of a state as Home Assistant writes it ("21.4", "21,4"); NAN for "unavailable", "unknown" or "".
inline float parse_number(const char *text) {
  if (!text || !*text) return NAN;
  char buf[24];
  size_t n = 0;
  for (; text[n] && n < sizeof(buf) - 1; ++n) buf[n] = text[n] == ',' ? '.' : text[n];
  buf[n] = 0;
  char *end = nullptr;
  const float v = strtof(buf, &end);
  return end == buf || std::isnan(v) || std::isinf(v) ? NAN : v;
}

// ---- Units ----

// A temperature of an attribute, which comes without a unit: Home Assistant's Plant Monitor sends it in the unit of
// the house. Above 45 it cannot be degrees Celsius in a room (the range options stop there), so it is Fahrenheit.
inline float celsius_of_attribute(float t) {
  return !std::isnan(t) && t > config::FAHRENHEIT_ABOVE ? (t - 32.0f) * 5.0f / 9.0f : t;
}

// ---- Plant Monitor ----

// The entity id Plant Monitor gives a plant of this name, as Home Assistant makes one: "Ficus Lyrata" is
// plant.ficus_lyrata. shortcut: a letter outside ASCII becomes "_" where Home Assistant would spell it in ASCII ("è" is
// "e"), and a renamed entity id is not found; drop it once Tessera hands a plugin tile its plant entity.
inline void plant_entity_of(const char *name, char *out, size_t size) {
  size_t n = snprintf(out, size, "plant.");
  bool gap = false;
  for (const char *c = name; *c && n + 1 < size; ++c) {
    const char lower = static_cast<char>(*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c);
    if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9')) {
      if (gap && n > 6 && n + 2 < size) out[n++] = '_';
      gap = false;
      out[n++] = lower;
    } else {
      gap = true;
    }
  }
  out[n] = 0;
}

// ---- The screen's own waterings ----

// The text entity "Plant watered at" keeps the waterings of the plants without a helper, one per tile:
// "3=1791703800,5=1791617400". "*" is every plant (the settings page's "Watered now"); a bare number, as version 0.1.0
// wrote it before release, counts as "*". The watering of `tile` is its own or "*", whichever is later; tile -1 asks
// for the latest of all.
inline uint32_t stored_watering(const char *store, int tile) {
  uint32_t best = 0;
  if (!store) return 0;
  for (const char *p = store; *p;) {
    const char *end = strchr(p, ',');
    if (!end) end = p + strlen(p);
    const char *eq = static_cast<const char *>(memchr(p, '=', static_cast<size_t>(end - p)));
    const char *value = eq ? eq + 1 : p;
    const bool every = !eq || (eq - p == 1 && *p == '*');
    const bool mine = !every && tile >= 0 && atoi(p) == tile;
    if (tile < 0 || every || mine) {
      const uint32_t v = static_cast<uint32_t>(strtoul(value, nullptr, 10));
      if (v > best) best = v;
    }
    p = *end ? end + 1 : end;
  }
  return best;
}

// `store` with the watering of `tile` (-1: "*", every plant) set to `epoch`, in `out`. "*" replaces every entry, since
// it is later than all of them. The oldest entries give way when `out` is full.
inline void store_watering(const char *store, int tile, uint32_t epoch, char *out, size_t size) {
  char key[12];
  if (tile < 0) snprintf(key, sizeof(key), "*");
  else snprintf(key, sizeof(key), "%d", tile);
  // The other tiles' entries, newest last as they were written, then this one.
  const char *keep[24];
  size_t lengths[24], count = 0;
  if (store && tile >= 0) {
    for (const char *p = store; *p && count < 24;) {
      const char *end = strchr(p, ',');
      if (!end) end = p + strlen(p);
      const size_t n = static_cast<size_t>(end - p);
      const size_t k = strlen(key);
      const bool same = n > k && strncmp(p, key, k) == 0 && p[k] == '=';
      if (n && !same) {
        keep[count] = p;
        lengths[count++] = n;
      }
      p = *end ? end + 1 : end;
    }
  }
  char last[32];
  const int last_n = snprintf(last, sizeof(last), "%s=%lu", key, static_cast<unsigned long>(epoch));
  size_t first = 0, total = static_cast<size_t>(last_n);
  for (size_t i = 0; i < count; ++i) total += lengths[i] + 1;
  while (first < count && total + 1 > size) total -= lengths[first++] + 1;
  size_t at = 0;
  for (size_t i = first; i < count; ++i) {
    memcpy(out + at, keep[i], lengths[i]);
    at += lengths[i];
    out[at++] = ',';
  }
  snprintf(out + at, size - at, "%s", last);
}

// ---- Words for a reading ----

// "42%", "21.5°C", "850 lx", "1.2k lx", "12k lx". `degree` is the screen font's degree sign, "" when it has none.
// `compact` is for a cell too narrow for the whole text in the smallest font: no decimal of a temperature, no "lx".
inline void value_text(Metric metric, float value, bool light_percent, const char *degree, char *out, size_t size,
                       bool compact = false) {
  if (std::isnan(value)) {
    snprintf(out, size, "--");
    return;
  }
  switch (metric) {
    case TEMPERATURE:
      if (compact) snprintf(out, size, "%.0f%s", value, *degree ? degree : "C");
      else snprintf(out, size, "%.1f%sC", value, degree);
      return;
    case LIGHT: {
      const char *lx = compact ? "" : " lx";
      if (light_percent) snprintf(out, size, "%.0f%%", value);
      else if (value < 1000) snprintf(out, size, "%.0f%s", value, lx);
      else if (value < 10000) snprintf(out, size, "%.1fk%s", value / 1000, lx);
      else snprintf(out, size, "%.0fk%s", value / 1000, lx);
      return;
    }
    default: snprintf(out, size, "%.0f%%", value); return;
  }
}

}  // namespace esphome::microplant
