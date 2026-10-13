#pragma once
namespace esphome::switch_ {
class Switch { public: bool state = false; void turn_on() { state = true; } void turn_off() { state = false; } };
}
