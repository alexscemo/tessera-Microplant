#pragma once
// The macros of esphome/core/helpers.h that real builds see, so a name that clashes with one fails here too.
#define HOT __attribute__((hot))
#define ALWAYS_INLINE __attribute__((always_inline))
#define PACKED __attribute__((packed))
namespace esphome {
namespace setup_priority { constexpr float DATA = 600.0f; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual float get_setup_priority() const { return 0; }
};
}  // namespace esphome
