#pragma once
// The plant itself: a pot with a face, a stem and four leaves, drawn from LVGL objects in a square. Its pose follows the
// plant's mood (leaves up or drooping, a smile or a frown, a sweat drop, closed eyes), and it moves now and then: it
// blinks, sways, and jumps with sparkles and a drop of water when it is watered.
//
// Every object is made in create(), inside the tile's parent, and the core deletes them with the tile. The animations
// act only on their own LVGL object (never on this class), and LVGL ends an object's animations when it is deleted, so
// nothing outlives the tile.
#include "esphome/components/smart_display/plugin_api.h"
#include "microplant_model.h"

namespace esphome::microplant {

class PlantAvatar {
 public:
  // Make the plant in a square of `size` pixels at (x, y) of `parent`.
  void create(lv_obj_t *parent, int x, int y, int size);
  // The pose of a mood; the same mood again changes nothing.
  void set_mood(Mood mood);
  // The colours of the look (the plant's colours are the same in light and dark, its ground is not).
  void on_theme();
  // Once a second: a blink now and then, a sway, a shiver when cold. `quiet` (standby) keeps it still.
  void on_tick(uint32_t epoch, bool quiet);
  // The thank you: a jump, a drop of water falling into the pot and sparkles around the leaves.
  void celebrate();
  // Whether a point of the parent lies on the plant (with a finger's margin).
  bool hit(int px, int py) const;
  bool made() const { return root_ != nullptr; }

  struct Pose;  // how the plant stands in a mood (microplant_avatar.cpp)

 private:
  void apply(const Pose &pose);
  void place_line(lv_obj_t *line, lv_point_precise_t *points, int count);
  int at(float unit) const { return static_cast<int>(unit * size_ + 0.5f); }

  lv_obj_t *root_{}, *foliage_{};
  lv_obj_t *stem_{}, *leaf_[4][3]{}, *flower_{}, *flower_mid_{};  // a leaf: its stalk, its body, its tip
  lv_obj_t *pot_{}, *rim_{}, *eye_[2]{}, *cheek_[2]{}, *mouth_{};
  lv_obj_t *sweat_{}, *drop_{}, *spark_[4]{}, *note_{};
  lv_point_precise_t stem_pts_[3]{}, leaf_pts_[4][4]{}, mouth_pts_[5]{};
  int x_ = 0, y_ = 0, size_ = 0, eye_h_ = 0;
  Mood mood_ = Mood::WAITING;
  bool posed_ = false;
};

}  // namespace esphome::microplant
