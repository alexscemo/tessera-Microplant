// A host stand-in for Tessera's core: the plugin API functions, LVGL with the screen's fonts (Roboto and the MDI icons
// through TinyTTF), and pages laid out like the 4-inch Guition (480 x 480, 2 x 3 grid, standard look at 170 dpi).
// Renders PPM frames of the MicroPlant tile in many states, light and dark, and the metric card.
#include "esphome/components/smart_display/plugin_api.h"
#include "../../components/microplant/microplant.h"
#include "../../components/microplant/microplant_shapes.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

using namespace esphome;
using namespace esphome::microplant;

// ---------------------------------------------------------------- the core's lists and functions
namespace tessera {
static uint32_t g_epoch = 0;
static Point g_tap;
static uint32_t g_next_re = 1;
struct Sent { uint32_t re; std::string json; };
static std::vector<Sent> g_sent;
static std::vector<std::string> g_actions;
struct Opened { std::string card, entity, title; int tile; };
static std::vector<Opened> g_opened;
static const lv_font_t *g_fonts[8];

Plugin::Plugin() { plugins().push_back(this); }
std::vector<Plugin *> &plugins() { static std::vector<Plugin *> v; return v; }
std::vector<TileType> &tile_types() { static std::vector<TileType> v; return v; }
std::vector<CardType> &card_types() { static std::vector<CardType> v; return v; }
std::vector<TapAction> &tap_actions() { static std::vector<TapAction> v; return v; }
std::vector<BarItemType> &bar_items() { static std::vector<BarItemType> v; return v; }
const TileType *tile_type(const std::string &key) {
  for (auto &t : tile_types()) if (t.key == key) return &t;
  return nullptr;
}
void Plugin::add_tile(const char *id, std::function<Tile *()> make) {
  tile_types().push_back({this, id, std::string("plugin:") + id_ + "." + id, make});
}
void Plugin::add_card(const char *id, std::function<Card *()> make, bool wide) {
  card_types().push_back({this, id, std::string("plugin:") + id_ + "." + id, wide, make});
}
void Plugin::add_tap_action(const char *id, std::function<void(const TapContext &)> run) {
  tap_actions().push_back({this, id, id, run});
}
void Plugin::add_bar_item(const char *id, std::function<BarItem()> read) { bar_items().push_back({this, id, id, read}); }
const char *Plugin::text(const char *key) const {
  for (auto &t : texts_) if (strcmp(t.first, key) == 0) return t.second;
  fprintf(stderr, "MISSING TEXT KEY %s\n", key);
  return "";
}
uint16_t Plugin::memory(const std::string &) const { return 1024; }

uint32_t epoch() { return g_epoch; }
static const int TZ = 3600;  // London in October: BST
LocalTime local_time(uint32_t e) {
  time_t t = e + TZ;
  struct tm *g = gmtime(&t);
  return {g->tm_year + 1900, g->tm_mon + 1, g->tm_mday, g->tm_hour, g->tm_min, g->tm_sec, g->tm_wday + 1};
}
std::string clock_text(uint32_t e) {
  auto t = local_time(e);
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", t.hour, t.minute);
  return b;
}
std::string format(const char *text, long n) {
  std::string s = text;
  auto bar = s.find(" | ");
  if (bar != std::string::npos) s = n == 1 ? s.substr(0, bar) : s.substr(bar + 3);
  auto at = s.find("{n}");
  if (at != std::string::npos) s.replace(at, 3, std::to_string(n));
  return s;
}
int32_t days_from_today(uint32_t e) {
  if (!g_epoch) return INT32_MIN;
  return static_cast<int32_t>((int64_t(e) + TZ) / 86400 - (int64_t(g_epoch) + TZ) / 86400);
}
std::string date_text(uint32_t e) {
  static const char *D[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *M[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  auto t = local_time(e);
  return std::string(D[t.weekday - 1]) + " " + std::to_string(t.day) + " " + M[t.month - 1];
}
std::string days_text(int32_t d) { return d == 1 ? "Tomorrow" : d > 1 ? "In " + std::to_string(d) + " days" : ""; }
std::string fill(const char *text, const char *name, const std::string &value) {
  std::string s = text, key = std::string("{") + name + "}";
  for (auto at = s.find(key); at != std::string::npos; at = s.find(key)) s.replace(at, key.size(), value);
  return s;
}
bool open_card(const char *, const char *card, const std::string &entity, int tile, const std::string &title) {
  g_opened.push_back({card, entity, title, tile});
  return true;
}
void close_card() {}
uint32_t send(const Plugin *, JsonObjectConst request) {
  std::string json;
  serializeJson(request, json);
  if (json.size() > 512) { fprintf(stderr, "REQUEST TOO LONG (%zu)\n", json.size()); return 0; }
  g_sent.push_back({g_next_re, json});
  return g_next_re++;
}
void refresh() {}
bool action(const char *service, const std::string &entity, const char *key, const std::string &value) {
  g_actions.push_back(std::string(service) + " " + entity + (key ? std::string(" ") + key + "=" + value : ""));
  return true;
}
Point tap_point() { return g_tap; }
SettingsPage &SettingsPage::toggle(const char *label, std::function<bool()> read, std::function<void(bool)> write) {
  Item i{}; i.kind = Item::TOGGLE; i.label = label; i.read = [read] { return read() ? 1 : 0; };
  i.write = [write](int v) { write(v != 0); }; items.push_back(i); return *this;
}
SettingsPage &SettingsPage::number(const char *label, int low, int high, int step, const char *unit,
                                   std::function<int()> read, std::function<void(int)> write) {
  Item i{}; i.kind = Item::NUMBER; i.label = label; i.low = low; i.high = high; i.step = step; i.unit = unit;
  i.read = read; i.write = write; items.push_back(i); return *this;
}
SettingsPage &SettingsPage::action(const char *label, const char *icon, std::function<void()> run, const char *confirm,
                                   std::function<std::string()> text) {
  Item i{}; i.kind = Item::ACTION; i.label = label; i.icon = icon; i.run = run; i.text = text; items.push_back(i);
  return *this;
}

namespace ui {
const lv_font_t *font(Font f) { return g_fonts[static_cast<int>(f)]; }
static uint32_t next_cp(const char *&s) {
  const unsigned char c = *s++;
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> n);
  while (n-- && *s) cp = (cp << 6) | (*s++ & 0x3F);
  return cp;
}
int text_width(const std::string &text, Font f) {
  lv_point_t size;
  lv_text_get_size(&size, text.c_str(), font(f), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}
bool covers(const std::string &text, Font f) {
  const char *s = text.c_str();
  while (*s) {
    uint32_t cp = next_cp(s);
    lv_font_glyph_dsc_t dsc;
    if (!lv_font_get_glyph_dsc(font(f), &dsc, cp, 0)) return false;
  }
  return true;
}
lv_obj_t *label(lv_obj_t *parent, Font f, theme::Role role) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_label_set_text(l, "");
  lv_obj_set_style_text_font(l, font(f), 0);
  lv_obj_set_height(l, lv_font_get_line_height(font(f)));
  lv_obj_set_style_text_color(l, theme::color(role), 0);
  return l;
}
lv_obj_t *text(lv_obj_t *parent, Font f, theme::Role role) { return label(parent, f, role); }
void set_text(lv_obj_t *l, const std::string &t) {
  if (l && t != lv_label_get_text(l)) lv_label_set_text(l, t.c_str());
}
void set_font(lv_obj_t *l, Font f) { lv_obj_set_style_text_font(l, font(f), 0); }
void set_color(lv_obj_t *l, theme::Role role) { lv_obj_set_style_text_color(l, theme::color(role), 0); }
static bool tone(Tone t, uint32_t &c) {
  switch (t) {
    case Tone::ACCENT: c = theme::hex(theme::ACCENT); return true;
    case Tone::BUSY: c = theme::foreground(theme::ha::AMBER); return true;
    case Tone::ALERT: c = theme::foreground(theme::ha::RED); return true;
    case Tone::GOOD: c = theme::foreground(theme::ha::GREEN); return true;
    default: return false;
  }
}
lv_color_t tone_color(Tone t, theme::Role normal) {
  uint32_t c;
  return tone(t, c) ? lv_color_hex(c) : theme::color(normal);
}
void set_tone(lv_obj_t *l, Tone t, theme::Role normal) { lv_obj_set_style_text_color(l, tone_color(t, normal), 0); }
void set_fill(lv_obj_t *b, Tone t, theme::Role normal) { lv_obj_set_style_bg_color(b, tone_color(t, normal), 0); }
lv_obj_t *block(lv_obj_t *parent, theme::Role fill) {
  lv_obj_t *b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::color(fill), 0);
  lv_obj_set_style_radius(b, ::ui::px(12), 0);
  return b;
}
std::string icon(uint32_t cp) {
  char b[5] = {};
  b[0] = static_cast<char>(0xF0 | (cp >> 18));
  b[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
  b[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
  b[3] = static_cast<char>(0x80 | (cp & 0x3F));
  return b;
}
bool pictures() { return true; }
}  // namespace ui
}  // namespace tessera

// ---------------------------------------------------------------- LVGL on the host
static const int W = 480, H = 480;
static std::vector<uint32_t> fb(W * H);
static uint32_t g_ms = 0;
static uint32_t tick() { return g_ms; }
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  for (int y = a->y1; y <= a->y2; ++y)
    memcpy(&fb[y * W + a->x1], px + ((y - a->y1) * (a->x2 - a->x1 + 1)) * 4, (a->x2 - a->x1 + 1) * 4);
  lv_display_flush_ready(d);
}
static std::vector<uint8_t> slurp(const char *path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static std::vector<std::vector<uint8_t>> font_data;
static const lv_font_t *ttf(const char *path, int size) {
  font_data.push_back(slurp(path));
  return lv_tiny_ttf_create_data(font_data.back().data(), font_data.back().size(), size);
}
static void save(const std::string &name) {
  lv_refr_now(nullptr);
  std::string path = "out/" + name + ".ppm";
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (uint32_t p : fb) { uint8_t rgb[3] = {uint8_t(p >> 16), uint8_t(p >> 8), uint8_t(p)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
static void advance(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) { g_ms += 10; lv_timer_handler(); }
}

// ---------------------------------------------------------------- the page: a 2 x 3 grid on the Guition
static const int MARGIN = 16, GAP = 12, PAD = 12, TOP = 56, CELL_W = (W - 2 * MARGIN - GAP) / 2;
static const int CELL_H = (H - TOP - MARGIN - 2 * GAP) / 3;
static lv_obj_t *screen_root;

struct Placed {
  std::unique_ptr<tessera::Tile> tile;
  lv_obj_t *parent;
  int x, y, w, h;  // the drawing area, on the glass
};
static std::vector<Placed> placed;

static void page(bool dark) {
  theme::dark = dark;
  for (auto &p : placed) p.tile.reset();
  placed.clear();
  if (screen_root) lv_obj_delete(screen_root);
  screen_root = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(screen_root);
  lv_obj_set_size(screen_root, W, H);
  lv_obj_set_style_bg_opa(screen_root, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screen_root, theme::color(theme::PAGE), 0);
  lv_obj_t *bar = tessera::ui::label(screen_root, tessera::Font::BODY_LARGE, theme::SLATE);
  lv_obj_set_pos(bar, MARGIN, 18);
  lv_obj_set_width(bar, W - 2 * MARGIN);
  lv_label_set_text(bar, "Living room                                        18:46");
}


static tessera::Tile *place(int col, int row, int cols, int rows, const std::string &options_json, const char *entity,
                            const char *name, const std::string &state_json) {
  const int x = MARGIN + col * (CELL_W + GAP), y = TOP + row * (CELL_H + GAP);
  const int w = cols * CELL_W + (cols - 1) * GAP, h = rows * CELL_H + (rows - 1) * GAP;
  lv_obj_t *card = lv_obj_create(screen_root);
  lv_obj_remove_style_all(card);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(card, theme::color(theme::CARD), 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(theme::outline(theme::hex(theme::CARD))), 0);
  lv_obj_t *area = lv_obj_create(card);
  lv_obj_remove_style_all(area);
  lv_obj_remove_flag(area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(area, PAD, PAD);
  lv_obj_set_size(area, w - 2 * PAD, h - 2 * PAD);
  auto &type = tessera::tile_types()[0];
  Placed p{std::unique_ptr<tessera::Tile>(type.make()), area, x + PAD, y + PAD, w - 2 * PAD, h - 2 * PAD};
  JsonDocument options;
  deserializeJson(options, options_json);
  tessera::TileContext c{area, p.w, p.h, uint8_t(cols), uint8_t(rows), name, entity, int(placed.size()),
                         options.as<JsonObjectConst>()};
  p.tile->create(c);
  JsonDocument state;
  deserializeJson(state, state_json);
  p.tile->on_state(state.as<JsonObjectConst>());
  p.tile->on_tick(tessera::g_epoch);
  tessera::Tile *t = p.tile.get();
  placed.push_back(std::move(p));
  return t;
}

// The app's answer to the last question of the tiles: the polled sensors.
static void answer_polls(Microplant &plugin, const std::map<std::string, std::string> &states,
                         const std::map<std::string, std::string> &units) {
  for (auto &s : tessera::g_sent) {
    if (s.json.find("history/history_during_period") == std::string::npos) continue;
    JsonDocument req;
    deserializeJson(req, s.json);
    JsonDocument m;
    m["re"] = s.re;
    m["ok"] = true;
    for (JsonVariant id : req["data"]["entity_ids"].as<JsonArray>()) {
      std::string e = id.as<const char *>();
      auto it = states.find(e);
      if (it == states.end()) continue;
      JsonArray rows = m["result"][e].to<JsonArray>();
      JsonObject row = rows.add<JsonObject>();
      row["s"] = it->second;
      row["lu"] = tessera::g_epoch - 30;
      auto u = units.find(e);
      if (u != units.end()) row["a"]["unit_of_measurement"] = u->second;
      if (e.rfind("input_datetime.", 0) == 0) row["a"]["timestamp"] = tessera::g_epoch - 2 * 86400 - 3 * 3600;
    }
    plugin.on_message(m.as<JsonObjectConst>());
  }
  tessera::g_sent.clear();
}

static const char *FULL_OPTIONS =
    R"({"temperature_entity":"sensor.monstera_temperature","humidity_entity":"sensor.living_room_humidity",)"
    R"("light_entity":"sensor.monstera_illuminance","watered_entity":"input_datetime.monstera_watered",)"
    R"("moisture_low":30,"moisture_high":70,"temperature_low":16,"temperature_high":28,"humidity_low":40,)"
    R"("humidity_high":70,"light_low":1000,"light_high":25000})";
static const char *ATTR_OPTIONS =
    R"({"moisture_low":30,"moisture_high":70,"temperature_low":16,"temperature_high":28,"humidity_low":40,)"
    R"("humidity_high":70,"light_low":1000,"light_high":25000})";

// A template sensor or a Plant Monitor entity with every reading as an attribute.
static std::string attrs(float soil, float temp, float hum, float lux, const char *species = nullptr) {
  char b[320], sp[80] = "";
  if (species) snprintf(sp, sizeof sp, R"(,"species":"%s")", species);
  snprintf(b, sizeof b,
           R"({"state":"%.0f","name":"Plant","attributes":{"moisture":%.0f,"temperature":%.1f,"humidity":%.0f,"illuminance":%.0f,"watered_at":%u%s}})",
           soil, soil, temp, hum, lux, tessera::g_epoch - 2 * 86400, sp);
  return b;
}

// The screen's fonts come from a checkout of Tessera's dev branch (deps/tessera-dev, or TESSERA_DEV).
static std::string font_path(const char *name) {
  const char *root = getenv("TESSERA_DEV");
  return std::string(root ? root : "deps/tessera-dev") + "/fonts/" + name;
}

int main() {
  ::ui::configure(170, "standard");
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *disp = lv_display_create(W, H);
  static std::vector<uint8_t> buf(W * H * 4);
  lv_display_set_buffers(disp, buf.data(), nullptr, buf.size(), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
  lv_display_set_flush_cb(disp, flush);
  const std::string r4 = font_path("Roboto-400.ttf"), r5 = font_path("Roboto-500.ttf"), r7 = font_path("Roboto-700.ttf");
  const char *R4 = r4.c_str(), *R5 = r5.c_str(), *R7 = r7.c_str(), *MDI = "deps/mdi/fonts/materialdesignicons-webfont.ttf";
  using tessera::Font;
  tessera::g_fonts[int(Font::VALUE)] = ttf(R5, 38);
  tessera::g_fonts[int(Font::HEADLINE)] = ttf(R5, 27);
  tessera::g_fonts[int(Font::TITLE)] = ttf(R7, 18);
  tessera::g_fonts[int(Font::BODY_LARGE)] = ttf(R4, 21);
  tessera::g_fonts[int(Font::BODY)] = ttf(R4, 16);
  tessera::g_fonts[int(Font::ICON)] = ttf(MDI, 42);
  tessera::g_fonts[int(Font::ICON_SMALL)] = ttf(MDI, 26);
  tessera::g_fonts[int(Font::DIGITS)] = ttf(R4, 64);

  // The plugin, as ESPHome would make it.
  switch_::Switch demo;
  number::Number poll;
  text::Text watered;
  Microplant plugin;
  plugin.set_identity("microplant", "0.1.0");
  {
    std::ifstream f("../../translations/en.json");
    static JsonDocument tr;
    deserializeJson(tr, f);
    static std::vector<std::string> keep;
    keep.reserve(400);
    for (JsonPair kv : tr["screen"].as<JsonObject>()) {
      keep.push_back(kv.key().c_str());
      const char *k = keep.back().c_str();
      keep.push_back(kv.value().as<const char *>());
      plugin.set_text(k, keep.back().c_str());
    }
  }
  plugin.set_demo_switch(&demo);
  plugin.set_poll_number(&poll);
  plugin.set_watered_text(&watered);
  plugin.setup();

  // Tuesday 13 October 2026, 18:46 in London.
  tessera::g_epoch = 1791999960;
  const std::map<std::string, std::string> UNITS = {{"sensor.monstera_temperature", "\xC2\xB0" "C"},
                                                     {"sensor.living_room_humidity", "%"},
                                                     {"sensor.monstera_illuminance", "lx"}};
  for (bool dark : {false, true}) {
    const std::string look = dark ? "dark" : "light";

    // 1. A new tile without an entity: the demo plant, parched (watered three days ago).
    page(dark);
    place(0, 0, 2, 3, ATTR_OPTIONS, "", "Monstera", "{}");
    save("full-demo-" + look);

    // 2. Thriving: the soil sensor is the tile's entity, the rest polled through the app.
    page(dark);
    auto *t = place(0, 0, 2, 3, FULL_OPTIONS, "sensor.monstera_soil_moisture", "Monstera",
                    R"({"state":"48","name":"Monstera soil","attributes":{"unit_of_measurement":"%"}})");
    answer_polls(plugin, {{"sensor.monstera_temperature", "22.4"}, {"sensor.living_room_humidity", "55"},
                          {"sensor.monstera_illuminance", "8200"}, {"input_datetime.monstera_watered", "x"}}, UNITS);
    t->on_tick(tessera::g_epoch);
    save("full-thriving-" + look);
    // 3. A tap on the plant: watered, the thank you, mid-jump.
    tessera::g_tap = {80, 120};
    t->on_tap();
    advance(260);
    save("full-thanks-" + look);
    advance(700);
    save("full-thanks2-" + look);
    for (auto &a : tessera::g_actions) printf("action: %s\n", a.c_str());
    tessera::g_actions.clear();
    // A tap on the light cell opens its card.
    tessera::g_tap = {380, 330};
    t->on_tap();

    // 4. Every mood at 1x1 and 2x1, from attributes of one entity.
    page(dark);
    place(0, 0, 1, 1, ATTR_OPTIONS, "sensor.plant_a", "Fern", attrs(52, 31.4, 55, 9000));    // hot
    place(1, 0, 1, 1, ATTR_OPTIONS, "sensor.plant_b", "Cactus", attrs(52, 12.5, 55, 9000));  // cold
    place(0, 1, 2, 1, ATTR_OPTIONS, "sensor.plant_c", "Pothos", attrs(22, 21, 55, 9000));    // thirsty
    place(0, 2, 2, 1, ATTR_OPTIONS, "sensor.plant_d", "Calathea", attrs(50, 21, 55, 260));   // dark
    save("small-moods-" + look);

    // 5. 2x2 and 2x1.
    page(dark);
    place(0, 0, 2, 2, ATTR_OPTIONS, "plant.ficus", "Ficus", attrs(84, 21, 55, 9000));        // soggy
    place(0, 2, 2, 1, ATTR_OPTIONS, "sensor.plant_e", "Basil", attrs(45, 22, 30, 60000));    // scorched
    save("wide-" + look);

    // 6. Tall tiles: 1x3 and 1x2.
    page(dark);
    place(0, 0, 1, 3, ATTR_OPTIONS, "sensor.plant_f", "Monstera", attrs(48, 22, 55, 9000));
    place(1, 0, 1, 2, ATTR_OPTIONS, "sensor.plant_g", "Orchid", attrs(40, 22, 28, 9000));    // dry air
    place(1, 2, 1, 1, ATTR_OPTIONS, "sensor.plant_h", "Ivy", attrs(9, 22, 55, 9000));        // parched
    save("tall-" + look);

    // 8. Every kind of plant (by its option), well, thirsty and parched, three to a page; a page of each; and kinds by
    // species in other moods.
    const float SOIL[3] = {50, 22, 9};
    const char *STEP[3] = {"good", "thirsty", "parched"};
    for (int k = 0; k < 3; ++k)
      for (int first = 0; first < SHAPE_COUNT; first += 3) {
        page(dark);
        for (int i = first; i < first + 3 && i < SHAPE_COUNT; ++i)
          place(0, i - first, 2, 1, std::string(R"({"shape":")") + SHAPES[i].id + "\"}", "sensor.plant_s",
                SHAPES[i].id, attrs(SOIL[k], 22, 55, 9000));
        save(std::string("kinds-") + STEP[k] + char('0' + first / 3) + "-" + look);
      }
    for (int i = 0; i < SHAPE_COUNT; ++i) {
      page(dark);
      place(0, 0, 2, 3, std::string(R"({"shape":")") + SHAPES[i].id + "\"}", "sensor.plant_s", SHAPES[i].id,
            attrs(50, 22, 55, 9000));
      save(std::string("kind-full-") + SHAPES[i].id + "-" + look);
    }
    page(dark);
    place(0, 0, 1, 1, R"({"shape":"auto"})", "sensor.plant_s", "Plant", attrs(84, 21, 55, 9000, "Monstera deliciosa"));
    place(1, 0, 1, 1, "{}", "sensor.plant_s", "Plant", attrs(50, 12, 55, 9000, "Sansevieria trifasciata"));  // cold
    place(0, 1, 1, 1, "{}", "sensor.plant_s", "Plant", attrs(50, 31, 55, 9000, "Chlorophytum comosum"));  // hot
    place(1, 1, 1, 1, "{}", "sensor.plant_s", "Plant", attrs(50, 22, 55, 260, "Echeveria elegans"));  // dark
    place(0, 2, 1, 1, "{}", "sensor.plant_s", "Plant", attrs(50, 22, 55, 9000, "Epipremnum aureum"));
    place(1, 2, 1, 1, "{}", "sensor.plant_s", "Plant", attrs(50, 12, 55, 9000, "Phalaenopsis"));  // cold
    save("kinds-moods-" + look);

    // 7. The cards, over a page of their own: soil with history, light of the demo.
    for (int which = 0; which < 2; ++which) {
      page(dark);
      lv_obj_t *frame = lv_obj_create(screen_root);
      lv_obj_remove_style_all(frame);
      lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_pos(frame, MARGIN, 72);
      lv_obj_set_size(frame, W - 2 * MARGIN, H - 72 - MARGIN);
      Detail &d = plugin.detail_;
      d = Detail{};
      d.metric = which == 0 ? MOISTURE : LIGHT;
      d.range = which == 0 ? Range{30, 70} : Range{1000, 25000};
      d.value = which == 0 ? 41 : 1800;
      d.demo = which == 1;
      d.demo_watered = tessera::g_epoch - 300;
      d.tile = 0;
      snprintf(d.entity, sizeof d.entity, "%s", which == 0 ? "sensor.monstera_soil_moisture" : "");
      lv_obj_t *title = tessera::ui::label(screen_root, Font::HEADLINE, theme::INK);
      lv_obj_set_pos(title, 0, 20);
      lv_obj_set_width(title, W);
      lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_text(title, which == 0 ? "Soil moisture" : "Light");
      std::unique_ptr<tessera::Card> card(tessera::card_types()[0].make());
      tessera::CardContext cc{frame, W - 2 * MARGIN, H - 72 - MARGIN, d.entity, 0};
      card->open(cc);
      card->on_tick(tessera::g_epoch);
      if (which == 0) {
        for (auto &s : tessera::g_sent) {
          if (s.json.find("statistics") == std::string::npos) continue;
          printf("card asked: %s\n", s.json.c_str());
          JsonDocument m;
          m["re"] = s.re;
          m["ok"] = true;
          JsonArray means = m["result"]["means"].to<JsonArray>();
          for (int i = 0; i < 24; ++i) means.add(i == 9 ? 74.0f : 62.0f - i * 1.4f + (i % 5) * 0.8f);
          plugin.on_message(m.as<JsonObjectConst>());
        }
        tessera::g_sent.clear();
      }
      save(std::string("card-") + (which ? "light-demo-" : "soil-") + look);
      lv_obj_delete(frame);
    }
  }
  tessera::SettingsPage sp;
  plugin.settings(sp);
  for (auto &it : sp.items) {
    printf("settings row: %s", it.label.c_str());
    if (it.read) printf(" = %d", it.read());
    if (it.text) printf(" [%s]", it.text().c_str());
    printf("\n");
  }
  sp.items[1].write(120);
  printf("poll after set 120: %u\n", plugin.poll_seconds());
  sp.items[2].run();
  printf("after Watered now: [%s]\n", sp.items[2].text().c_str());
  for (auto &o : tessera::g_opened) printf("opened: %s entity=%s title=%s tile=%d\n", o.card.c_str(), o.entity.c_str(), o.title.c_str(), o.tile);
  printf("screen watered: '%s'\n", watered.state.c_str());
  page(false);
  place(0, 0, 1, 1, ATTR_OPTIONS, "sensor.plant_h", "Ivy", attrs(9, 22, 55, 9000));
  auto item = tessera::bar_items()[0].read();
  printf("bar with a parched Ivy: shown=%d tone=%d text=%s\n", item.shown, int(item.tone), item.text.c_str());
  page(false);
  item = tessera::bar_items()[0].read();
  printf("bar: shown=%d text=%s\n", item.shown, item.text.c_str());
  return 0;
}
