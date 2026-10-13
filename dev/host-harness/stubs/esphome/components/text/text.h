#pragma once
#include <string>
namespace esphome::text {
class Text;
class TextCall { public: explicit TextCall(Text *t) : t_(t) {} TextCall &set_value(const std::string &v) { v_ = v; return *this; } void perform(); private: Text *t_; std::string v_; };
class Text { public: std::string state; TextCall make_call() { return TextCall(this); } };
inline void TextCall::perform() { t_->state = v_; }
}
