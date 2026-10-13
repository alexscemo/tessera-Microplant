#include "microplant_avatar.h"
#include <algorithm>
#include <cmath>

namespace esphome::microplant {

namespace ui = tessera::ui;

// ---- Animations: each acts on its own object only ----

namespace {

void lift(void *var, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(var), v, 0); }

// A sway around the middle: v runs 0..1000 over one swing, the user data is the reach in pixels.
void sway(lv_anim_t *a, int32_t v) {
  const int reach = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  const float x = std::sin(6.2831853f * static_cast<float>(v) / 1000.0f) * static_cast<float>(reach);
  lv_obj_set_style_translate_x(static_cast<lv_obj_t *>(a->var), static_cast<int32_t>(x), 0);
}

// A blink: the eye's height, kept on its own centre line (the user data).
void blink(lv_anim_t *a, int32_t h) {
  auto *eye = static_cast<lv_obj_t *>(a->var);
  const int cy = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  lv_obj_set_height(eye, h);
  lv_obj_set_y(eye, cy - h / 2);
}

// A sparkle: it shows and rises a little, then fades back (v 0..255 and back).
void sparkle(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  const int rise = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  lv_obj_set_style_opa(o, static_cast<lv_opa_t>(v), 0);
  lv_obj_set_style_translate_y(o, -rise * v / 255, 0);
}

// A drop that falls (v is how far, the user data the whole way) and fades out on its last fifth.
void fall(lv_anim_t *a, int32_t v) {
  auto *o = static_cast<lv_obj_t *>(a->var);
  const int way = static_cast<int>(reinterpret_cast<intptr_t>(lv_anim_get_user_data(a)));
  lv_obj_set_style_translate_y(o, v, 0);
  const int fade_from = way * 4 / 5;
  const int opa = v < fade_from || way <= fade_from ? 255 : 255 - 255 * (v - fade_from) / (way - fade_from);
  lv_obj_set_style_opa(o, static_cast<lv_opa_t>(opa < 0 ? 0 : opa), 0);
}

void start(lv_obj_t *o, int32_t from, int32_t to, uint32_t ms, lv_anim_exec_xcb_t plain, lv_anim_custom_exec_cb_t custom,
           intptr_t user, uint32_t delay = 0, uint32_t back_ms = 0, uint32_t repeat = 1) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay);
  if (back_ms) lv_anim_set_reverse_duration(&a, back_ms);
  lv_anim_set_repeat_count(&a, repeat);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  if (plain) lv_anim_set_exec_cb(&a, plain);
  if (custom) lv_anim_set_custom_exec_cb(&a, custom);
  lv_anim_set_user_data(&a, reinterpret_cast<void *>(user));
  lv_anim_start(&a);
}

lv_obj_t *box(lv_obj_t *parent, int x, int y, int size) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, size, size);
  return o;
}

lv_obj_t *dot(lv_obj_t *parent, theme::Role role) {
  lv_obj_t *o = ui::block(parent, role);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  return o;
}

lv_obj_t *stroke(lv_obj_t *parent, int size) {
  lv_obj_t *o = lv_line_create(parent);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_size(o, size, size);
  lv_obj_set_style_line_rounded(o, true, 0);
  return o;
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

// ---- The poses ----

enum class Mouth : uint8_t { SMILE, GRIN, FLAT, FROWN, OPEN, WAVY };

struct PlantAvatar::Pose {
  float droop;      // 0 leaves up, 1 hanging
  float tuck;       // 0 open, 1 leaves pulled in (cold)
  float eyes;       // 1 open, 0.3 a happy squint, 0.15 closed
  Mouth mouth;
  bool flower, cheeks, sweat;
  const char *note; // a sign beside the leaves: "z" asleep, "!" alarm
  theme::Role tint; // what the leaves turn towards (MARK_GREEN for none)
  uint8_t tint_mix; // how far, 0 to 255
};

static PlantAvatar::Pose pose_of(Mood mood);

void PlantAvatar::create(lv_obj_t *parent, int x, int y, int size) {
  x_ = x;
  y_ = y;
  size_ = size < 24 ? 24 : size;
  root_ = box(parent, x, y, size_);
  // The leaves first, so the pot covers the foot of the stem.
  foliage_ = box(root_, 0, 0, size_);
  stem_ = stroke(foliage_, size_);
  for (auto &leaf : leaf_)
    for (auto *&part : leaf) part = stroke(foliage_, size_);
  flower_ = dot(foliage_, theme::MARK_PURPLE);
  flower_mid_ = dot(foliage_, theme::MARK_AMBER);

  pot_ = ui::block(root_, theme::MARK_AMBER);
  lv_obj_remove_flag(pot_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(pot_, at(0.23f), at(0.64f));
  lv_obj_set_size(pot_, at(0.54f), at(0.33f));
  lv_obj_set_style_radius(pot_, at(0.08f), 0);
  rim_ = ui::block(root_, theme::MARK_AMBER);
  lv_obj_remove_flag(rim_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(rim_, at(0.17f), at(0.56f));
  lv_obj_set_size(rim_, at(0.66f), at(0.11f));
  lv_obj_set_style_radius(rim_, at(0.04f), 0);
  // A shade under the rim, so it reads as a lip over the pot.
  lv_obj_set_style_border_side(rim_, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(rim_, at(0.022f) < 2 ? 2 : at(0.022f), 0);
  lv_obj_set_style_border_opa(rim_, LV_OPA_20, 0);

  eye_h_ = at(0.075f);
  for (int i = 0; i < 2; ++i) {
    cheek_[i] = dot(root_, theme::MARK_PURPLE);
    lv_obj_set_size(cheek_[i], at(0.08f), at(0.045f));
    lv_obj_set_pos(cheek_[i], at(i ? 0.63f : 0.29f), at(0.835f));
    lv_obj_set_style_bg_opa(cheek_[i], LV_OPA_50, 0);
    eye_[i] = dot(root_, theme::QR_DARK);
    lv_obj_set_size(eye_[i], eye_h_, eye_h_);
    lv_obj_set_pos(eye_[i], at(i ? 0.59f : 0.41f) - eye_h_ / 2, at(0.765f) - eye_h_ / 2);
  }
  mouth_ = stroke(root_, size_);

  sweat_ = dot(root_, theme::MARK_BLUE);
  lv_obj_set_size(sweat_, at(0.06f), at(0.085f));
  drop_ = dot(root_, theme::MARK_BLUE);
  lv_obj_set_size(drop_, at(0.075f), at(0.1f));
  lv_obj_set_pos(drop_, at(0.5f) - at(0.0375f), 0);
  lv_obj_set_style_opa(drop_, LV_OPA_TRANSP, 0);
  static constexpr float SPARKS[4][2] = {{0.22f, 0.16f}, {0.76f, 0.12f}, {0.12f, 0.42f}, {0.84f, 0.40f}};
  for (int i = 0; i < 4; ++i) {
    spark_[i] = dot(root_, i % 2 ? theme::MARK_AMBER : theme::MARK_GREEN);
    const int d = at(i % 2 ? 0.05f : 0.04f);
    lv_obj_set_size(spark_[i], d, d);
    lv_obj_set_pos(spark_[i], at(SPARKS[i][0]) - d / 2, at(SPARKS[i][1]) - d / 2);
    lv_obj_set_style_opa(spark_[i], LV_OPA_TRANSP, 0);
  }
  const tessera::Font note_font = size_ >= ui::px(120) ? tessera::Font::HEADLINE : tessera::Font::TITLE;
  note_ = ui::label(root_, note_font, theme::MUTED);
  lv_obj_set_size(note_, at(0.2f), ui::line_height(note_font));
  lv_obj_set_pos(note_, at(0.76f), at(0.02f));

  on_theme();
  apply(pose_of(Mood::WAITING));
  mood_ = Mood::WAITING;
  posed_ = true;
}

static PlantAvatar::Pose pose_of(Mood mood) {
  using P = PlantAvatar::Pose;
  switch (mood) {
    case Mood::THRIVING: return P{0.0f, 0, 1.0f, Mouth::SMILE, true, true, false, "", theme::MARK_GREEN, 0};
    case Mood::THANKS: return P{-0.12f, 0, 0.3f, Mouth::GRIN, true, true, false, "", theme::MARK_GREEN, 0};
    case Mood::THIRSTY: return P{0.55f, 0, 0.85f, Mouth::FROWN, false, false, false, "", theme::MARK_AMBER, 60};
    case Mood::PARCHED: return P{1.0f, 0, 0.55f, Mouth::FROWN, false, false, false, "!", theme::MARK_AMBER, 140};
    case Mood::SOGGY: return P{0.35f, 0, 1.0f, Mouth::WAVY, false, false, true, "", theme::MARK_BLUE, 50};
    case Mood::TOO_HOT: return P{0.4f, 0, 0.9f, Mouth::OPEN, false, true, true, "", theme::MARK_AMBER, 50};
    case Mood::TOO_COLD: return P{0.1f, 1.0f, 0.7f, Mouth::WAVY, false, false, false, "", theme::MARK_BLUE, 90};
    case Mood::DARK: return P{0.25f, 0, 0.15f, Mouth::FLAT, false, false, false, "z", theme::MARK_GREEN, 0};
    case Mood::SCORCHED: return P{0.3f, 0, 0.3f, Mouth::FROWN, false, true, true, "", theme::MARK_AMBER, 90};
    case Mood::DRY_AIR: return P{0.25f, 0, 0.85f, Mouth::FLAT, false, false, false, "", theme::MARK_AMBER, 40};
    case Mood::HUMID: return P{0.2f, 0, 0.85f, Mouth::WAVY, false, false, false, "", theme::MARK_BLUE, 30};
    case Mood::WAITING:
    default: return P{0.15f, 0, 1.0f, Mouth::FLAT, false, false, false, "", theme::MARK_GREEN, 0};
  }
}

void PlantAvatar::set_mood(Mood mood) {
  if (!root_ || (posed_ && mood == mood_)) return;
  mood_ = mood;
  apply(pose_of(mood));
}

void PlantAvatar::place_line(lv_obj_t *line, lv_point_precise_t *points, int count) {
  lv_line_set_points(line, points, static_cast<uint32_t>(count));
  lv_obj_invalidate(line);
}

void PlantAvatar::apply(const Pose &p) {
  const float d = p.droop, t = p.tuck;
  // The stem bends over a little as the plant droops.
  const float bx = 0.5f, by = 0.6f;
  const float tx = 0.5f + 0.13f * d, ty = 0.2f + 0.13f * d;
  const float mx = lerp(bx, tx, 0.5f) + 0.012f * d, my = lerp(by, ty, 0.5f);
  stem_pts_[0] = {at(bx), at(by)};
  stem_pts_[1] = {at(mx), at(my)};
  stem_pts_[2] = {at(tx), at(ty)};
  lv_obj_set_style_line_width(stem_, std::max(2, at(0.04f)), 0);
  place_line(stem_, stem_pts_, 3);

  // Two pairs of leaves on the stem. A leaf is a thin stalk, a round body and a narrower tip along a line from its node
  // at an angle: raised when the plant does well, hanging when it droops, pulled up against the stem when it is cold.
  struct Leaf { float along, length, up, down, side; };
  static constexpr Leaf LEAVES[4] = {
      {0.38f, 0.33f, 16.0f, -44.0f, 1}, {0.38f, 0.33f, 16.0f, -44.0f, -1},
      {0.72f, 0.26f, 38.0f, -6.0f, 1}, {0.72f, 0.26f, 38.0f, -6.0f, -1}};
  const lv_color_t green = lv_color_mix(ui::color(p.tint), ui::color(theme::MARK_GREEN), p.tint_mix);
  // The upper pair a shade towards the card, so the two pairs read apart in both looks.
  const lv_color_t young = lv_color_mix(ui::color(theme::CARD), green, 70);
  for (int i = 0; i < 4; ++i) {
    const Leaf &leaf = LEAVES[i];
    const float nx = lerp(bx, tx, leaf.along), ny = lerp(by, ty, leaf.along);
    const float droop = std::clamp(d, -0.3f, 1.0f);
    float angle = lerp(leaf.up, leaf.down, droop);
    angle = lerp(angle, 72.0f, t);
    const float length = leaf.length * (1.0f - 0.2f * t);
    const float rad = angle * 0.0174533f;
    const float ux = std::cos(rad) * leaf.side, uy = -std::sin(rad);
    // The tip curls down as the leaf hangs, and a little up when it is raised.
    const float curl = 0.08f * droop - 0.02f;
    leaf_pts_[i][0] = {at(nx), at(ny)};
    leaf_pts_[i][1] = {at(nx + ux * length * 0.22f), at(ny + uy * length * 0.22f)};
    leaf_pts_[i][2] = {at(nx + ux * length * 0.62f), at(ny + uy * length * 0.62f)};
    leaf_pts_[i][3] = {at(nx + ux * length), at(ny + uy * length + curl)};
    static constexpr float WIDTHS[3] = {0.028f, 0.135f, 0.07f};
    for (int k = 0; k < 3; ++k) {
      lv_obj_t *part = leaf_[i][k];
      const float scale = i >= 2 ? 0.85f : 1.0f;
      lv_obj_set_style_line_width(part, std::max(k ? 3 : 2, at(WIDTHS[k] * scale)), 0);
      lv_obj_set_style_line_color(part, i >= 2 ? young : green, 0);
      place_line(part, &leaf_pts_[i][k], 2);
    }
  }
  lv_obj_set_style_line_color(stem_, green, 0);

  // A flower on top of a plant that does well.
  const int fd = at(0.15f), fm = at(0.06f);
  lv_obj_set_size(flower_, fd, fd);
  lv_obj_set_pos(flower_, at(tx) - fd / 2, at(ty) - fd / 2 - at(0.02f));
  lv_obj_set_size(flower_mid_, fm, fm);
  lv_obj_set_pos(flower_mid_, at(tx) - fm / 2, at(ty) - fm / 2 - at(0.02f));
  lv_obj_set_style_opa(flower_, p.flower ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_opa(flower_mid_, p.flower ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

  // The face.
  const int eh = std::max(2, static_cast<int>(eye_h_ * p.eyes + 0.5f));
  for (int i = 0; i < 2; ++i) {
    lv_anim_delete(eye_[i], nullptr);
    lv_obj_set_height(eye_[i], eh);
    lv_obj_set_y(eye_[i], at(0.765f) - eh / 2);
    lv_obj_set_style_opa(cheek_[i], p.cheeks ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
  static constexpr float SHAPES[6][5] = {
      {0.0f, 0.022f, 0.03f, 0.022f, 0.0f},      // smile
      {-0.01f, 0.03f, 0.045f, 0.03f, -0.01f},   // grin
      {0.015f, 0.015f, 0.015f, 0.015f, 0.015f}, // flat
      {0.035f, 0.012f, 0.004f, 0.012f, 0.035f}, // frown
      {0.015f, 0.015f, 0.015f, 0.015f, 0.015f}, // open: a round mouth, by its width
      {0.022f, 0.006f, 0.022f, 0.006f, 0.022f}, // wavy
  };
  const int shape = static_cast<int>(p.mouth);
  const float half = p.mouth == Mouth::GRIN ? 0.085f : p.mouth == Mouth::OPEN ? 0.006f : 0.07f;
  for (int i = 0; i < 5; ++i)
    mouth_pts_[i] = {at(0.5f - half + half * 0.5f * static_cast<float>(i)), at(0.85f + SHAPES[shape][i])};
  const int mw = at(p.mouth == Mouth::OPEN ? 0.06f : 0.024f);
  lv_obj_set_style_line_width(mouth_, mw < 2 ? 2 : mw, 0);
  place_line(mouth_, mouth_pts_, 5);

  // A drop of sweat beside the pot when hot, a puddle drop under it when soggy.
  const bool soggy = mood_ == Mood::SOGGY;
  lv_obj_set_pos(sweat_, at(soggy ? 0.84f : 0.78f), at(soggy ? 0.86f : 0.66f));
  lv_obj_set_style_opa(sweat_, p.sweat ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  ui::set_text(note_, p.note);
}

void PlantAvatar::on_theme() {
  if (!root_) return;
  // The plant keeps its colours in both looks (the mark's colours); the face is dark on the pot in both.
  lv_obj_set_style_border_color(rim_, ui::color(theme::QR_DARK), 0);
  lv_obj_set_style_line_color(mouth_, ui::color(theme::QR_DARK), 0);
  ui::set_color(note_, theme::MUTED);
  // The upper leaves mix with the card's colour, which the look changes: pose them again.
  if (posed_) apply(pose_of(mood_));
}

void PlantAvatar::on_tick(uint32_t epoch, bool quiet) {
  if (!root_ || quiet || !epoch) return;
  const bool asleep = mood_ == Mood::DARK;
  // A blink every seven seconds or so (never two eyes apart), but not with closed eyes.
  if (!asleep && epoch % 7 == 3) {
    const int h = lv_obj_get_height(eye_[0]);
    for (auto *eye : eye_)
      start(eye, h, 2, 90, nullptr, blink, at(0.765f), 0, 110);
  }
  // A sway of the leaves every eleven seconds; a quick shiver every four when cold.
  if (mood_ == Mood::TOO_COLD && epoch % 4 == 1) {
    start(foliage_, 0, 1000, 120, nullptr, sway, std::max(1, at(0.012f)), 0, 0, 5);
  } else if (!asleep && epoch % 11 == 5) {
    start(foliage_, 0, 1000, 2200, nullptr, sway, std::max(1, at(0.02f)));
  }
}

void PlantAvatar::celebrate() {
  if (!root_) return;
  // Three small jumps.
  lv_anim_delete(root_, nullptr);
  start(root_, 0, -at(0.07f), 160, lift, nullptr, 0, 0, 180, 3);
  // A drop falls from above into the pot.
  lv_anim_delete(drop_, nullptr);
  start(drop_, 0, at(0.5f), 520, nullptr, fall, at(0.5f));
  // Sparkles around the leaves, one after the other.
  for (int i = 0; i < 4; ++i) {
    lv_anim_delete(spark_[i], nullptr);
    start(spark_[i], 0, 255, 260, nullptr, sparkle, at(0.06f), 380 + 140 * i, 420);
  }
}

bool PlantAvatar::hit(int px, int py) const {
  if (!root_) return false;
  const int margin = ui::px(8);
  return px >= x_ - margin && px <= x_ + size_ + margin && py >= y_ - margin && py <= y_ + size_ + margin;
}

}  // namespace esphome::microplant
