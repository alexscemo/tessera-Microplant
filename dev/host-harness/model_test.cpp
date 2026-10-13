#include "../../components/microplant/microplant_model.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
using namespace esphome::microplant;
static uint32_t utc(int y,int mo,int d,int h,int mi,int s){ struct tm t{}; t.tm_year=y-1900;t.tm_mon=mo-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=mi;t.tm_sec=s; return timegm(&t);}
int main(){
  assert(parse_moment("2026-10-10 08:00:00", 3600) == utc(2026,10,10,7,0,0));       // input_datetime, local BST
  assert(parse_moment("2026-10-10T07:00:00.123456+00:00", 3600) == utc(2026,10,10,7,0,0));  // input_button
  assert(parse_moment("2026-10-10T09:00:00+02:00", 0) == utc(2026,10,10,7,0,0));
  assert(parse_moment("2026-10-10T07:00:00Z", 3600) == utc(2026,10,10,7,0,0));
  assert(parse_moment("2026-10-10", 0) == utc(2026,10,10,0,0,0));
  assert(parse_moment("unknown", 0) == 0 && parse_moment("", 0) == 0 && parse_moment(nullptr, 0) == 0);
  char b[32]; iso_utc(utc(2026,1,2,3,4,5), b, sizeof b); assert(std::string(b) == "2026-01-02T03:04:05+00:00");
  assert(parse_moment(b, 999) == utc(2026,1,2,3,4,5));
  assert(std::isnan(parse_number("unavailable")) && parse_number("21,5") == 21.5f && parse_number("48") == 48.f);
  Range soil{30,70};
  assert(evaluate(MOISTURE, 50, soil) == Status::GOOD);
  assert(evaluate(MOISTURE, 25, soil) == Status::LOW_WARN && evaluate(MOISTURE, 19, soil) == Status::LOW_BAD);
  assert(evaluate(MOISTURE, 75, soil) == Status::HIGH_WARN && evaluate(MOISTURE, 81, soil) == Status::HIGH_BAD);
  assert(evaluate(LIGHT, 700, {1000,25000}) == Status::LOW_WARN && evaluate(LIGHT, 400, {1000,25000}) == Status::LOW_BAD);
  assert(evaluate(TEMPERATURE, NAN, {16,28}) == Status::NONE);
  Status s[4] = {Status::LOW_WARN, Status::HIGH_BAD, Status::GOOD, Status::GOOD};
  assert(mood_of(s, false) == Mood::TOO_HOT);      // red beats amber, whatever the reading
  s[1] = Status::HIGH_WARN; assert(mood_of(s, false) == Mood::THIRSTY);  // soil first among ambers
  assert(mood_of(s, true) == Mood::THANKS);
  Status none[4] = {}; assert(mood_of(none, false) == Mood::WAITING);
  Status good[4] = {Status::GOOD,Status::GOOD,Status::GOOD,Status::GOOD}; assert(mood_of(good,false)==Mood::THRIVING && health_of(good)==100);
  // The demo: parched three days on, full after a watering, thirsty within ten minutes.
  const uint32_t now = 1791999960;
  assert(demo_value(MOISTURE, now, now - config::DEMO_FIRST_WATERED_AGO) == config::DEMO_MOISTURE_FLOOR);
  assert(demo_value(MOISTURE, now, now) == config::DEMO_MOISTURE_FULL);
  assert(demo_value(MOISTURE, now + 600, now) < 30);
  char v[24]; value_text(LIGHT, 1260, false, "", v, sizeof v); assert(std::string(v) == "1.3k lx");
  value_text(TEMPERATURE, 21.44f, false, "\xC2\xB0", v, sizeof v); assert(std::string(v) == "21.4\xC2\xB0" "C");
  // Units: a Plant Monitor temperature above 45 is Fahrenheit.
  assert(celsius_of_attribute(21.0f) == 21.0f && std::fabs(celsius_of_attribute(72.0f) - 22.2222f) < 0.01f);
  assert(std::isnan(celsius_of_attribute(NAN)));
  // The screen's waterings: one per tile, "*" for every plant, a bare number (the old form) counts as "*".
  auto put = [](const std::string &store, int tile, uint32_t epoch, size_t size = 64) {
    char out[64];
    store_watering(store.c_str(), tile, epoch, out, size);
    return std::string(out);
  };
  std::string w = put("", 3, 100);
  assert(w == "3=100");
  w = put(w, 5, 200);
  assert(w == "3=100,5=200");
  assert(stored_watering(w.c_str(), 3) == 100 && stored_watering(w.c_str(), 5) == 200 && stored_watering(w.c_str(), 4) == 0);
  assert(stored_watering(w.c_str(), -1) == 200);
  w = put(w, 3, 300);
  assert(w == "5=200,3=300");
  w = put(w, 13, 400);
  assert(stored_watering(w.c_str(), 1) == 0 && stored_watering(w.c_str(), 13) == 400);
  w = put(w, -1, 500);
  assert(w == "*=500" && stored_watering(w.c_str(), 7) == 500);
  assert(stored_watering("1791703800", 2) == 1791703800u);  // the old form
  assert(put("1791703800", 2, 1791800000) == "1791703800,2=1791800000");
  assert(put("1=100,2=200", 3, 300, 12) == "2=200,3=300");  // the oldest gives way
  puts("model: all good");
}
