#pragma once
namespace esphome::number {
class Number;
class NumberCall { public: explicit NumberCall(Number *n) : n_(n) {} NumberCall &set_value(float v) { v_ = v; return *this; } void perform(); private: Number *n_; float v_ = 0; };
class Number { public: float state = 60; NumberCall make_call() { return NumberCall(this); } };
inline void NumberCall::perform() { n_->state = v_; }
}
