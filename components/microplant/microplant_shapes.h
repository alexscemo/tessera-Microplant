#pragma once
// The kinds of plant the tile draws, as data: no LVGL here (the avatar draws them, model_test checks the matching).
//
// To add a kind: one entry in SHAPES (its id, the words of its species, its form), its choice in the manifest's option
// `shape` (value = the id) and the choice's label in translations/*.json. Look at it with the host harness.
#include <cstdint>
#include <cstring>

namespace esphome::microplant {

// A leaf is three lines from its node (on the stem at `along`, moved by `dx`), each to the next point along it: `cut`
// says where each line ends, as a part of `length`, and `width` how thick it is. It leaves at an angle from the level,
// `up` when the plant does well and `down` when it hangs; the later lines `turn`, the last one `arch`s further. All in
// parts of the plant's square. A length of 0 leaves the leaf out.
struct Leaf {
  float along, dx, length, up, down, side, turn, arch;
  float cut[3], width[3];
  bool young;  // a shade towards the card, to read apart from the leaves behind
};

// A form: its stem's width (0: none, the leaves grow from the soil, and no flower), how far the stem bends over as it
// droops, how far above the stem's top the flower sits (below 0: never a flower), whether a leaf's third line is a split cut into it (a
// monstera), and up to four leaves.
struct Form {
  float stem, lean, flower;
  bool cut;
  Leaf leaves[4];
};

// A kind: the id the option stores, the words (any case, space apart) that find it in a species, and its form.
struct ShapeKind {
  const char *id, *words;
  Form form;
};

// The first is the default. The order is the order of matching: a species that names two kinds takes the first.
inline constexpr ShapeKind SHAPES[] = {
    // Two pairs of leaves on the stem, each a thin stalk, a round body and a narrower tip.
    {"leafy", "", {0.04f, 0.13f, 0.02f, false, {
        {0.38f, 0, 0.33f, 16, -44, 1, 0, 0, {0.22f, 0.62f, 1}, {0.028f, 0.135f, 0.07f}, false},
        {0.38f, 0, 0.33f, 16, -44, -1, 0, 0, {0.22f, 0.62f, 1}, {0.028f, 0.135f, 0.07f}, false},
        {0.72f, 0, 0.26f, 38, -6, 1, 0, 0, {0.22f, 0.62f, 1}, {0.024f, 0.115f, 0.06f}, true},
        {0.72f, 0, 0.26f, 38, -6, -1, 0, 0, {0.22f, 0.62f, 1}, {0.024f, 0.115f, 0.06f}, true}}}},
    // Four big round leaves on long stalks from the soil, bowing at the stalk's end, each with a split.
    {"monstera", "monstera", {0, 0, 0, true, {
        {0, -0.05f, 0.5f, 52, 32, -1, -35, 0, {0.55f, 0.95f, 0.95f}, {0.03f, 0.22f, 0.03f}, false},
        {0, 0.05f, 0.5f, 50, 30, 1, -35, 0, {0.55f, 0.95f, 0.95f}, {0.03f, 0.22f, 0.03f}, false},
        {0, -0.02f, 0.6f, 84, 62, -1, -50, 0, {0.6f, 0.95f, 0.95f}, {0.03f, 0.2f, 0.03f}, true},
        {0, 0.02f, 0.52f, 76, 56, 1, -45, 0, {0.6f, 0.95f, 0.95f}, {0.03f, 0.19f, 0.03f}, true}}}},
    // Four upright blades from the soil, wide to a point, that fall outwards as it droops.
    {"snake", "snake sansevieria trifasciata", {0, 0, 0, false, {
        {0, -0.12f, 0.42f, 110, 145, 1, 0, 0, {0.35f, 0.8f, 1}, {0.1f, 0.105f, 0.05f}, false},
        {0, -0.04f, 0.56f, 96, 122, 1, 0, 0, {0.35f, 0.8f, 1}, {0.1f, 0.11f, 0.05f}, true},
        {0, 0.04f, 0.5f, 85, 58, 1, 0, 0, {0.35f, 0.8f, 1}, {0.1f, 0.11f, 0.05f}, false},
        {0, 0.12f, 0.38f, 72, 38, 1, 0, 0, {0.35f, 0.8f, 1}, {0.1f, 0.105f, 0.05f}, true}}}},
    // A round column (the stem) with two arms that go out, then up (out and down when it droops); the flower on top.
    {"cactus", "cact cereus opuntia echinopsis mammillaria", {0.21f, 0.04f, 0.09f, false, {
        {0.3f, 0, 0.4f, 0, -55, 1, 90, 0, {0.42f, 0.8f, 1}, {0.11f, 0.11f, 0.11f}, false},
        {0.55f, 0, 0.34f, 0, -55, -1, 90, 0, {0.4f, 0.8f, 1}, {0.11f, 0.11f, 0.11f}, false},
        {},
        {}}}},
    // Long thin leaves from the soil that arch over and down, striped by turns.
    {"spider", "spider chlorophytum", {0, 0, 0, false, {
        {0, -0.02f, 0.42f, 78, 45, -1, -25, -35, {0.4f, 0.75f, 1}, {0.05f, 0.045f, 0.03f}, false},
        {0, 0.02f, 0.42f, 74, 42, 1, -25, -35, {0.4f, 0.75f, 1}, {0.05f, 0.045f, 0.03f}, true},
        {0, -0.03f, 0.5f, 48, 18, -1, -30, -40, {0.4f, 0.75f, 1}, {0.05f, 0.045f, 0.03f}, true},
        {0, 0.03f, 0.5f, 45, 15, 1, -30, -40, {0.4f, 0.75f, 1}, {0.05f, 0.045f, 0.03f}, false}}}},
    // Two heart leaves up, and two vines over the rim with a leaf at their end.
    {"pothos", "pothos epipremnum scindapsus hedera ivy tradescantia", {0, 0, 0, false, {
        {0.1f, -0.18f, 0.55f, 18, -5, -1, -105, 0, {0.4f, 0.82f, 1}, {0.03f, 0.03f, 0.14f}, false},
        {0.1f, 0.18f, 0.5f, 18, -5, 1, -105, 0, {0.42f, 0.8f, 1}, {0.03f, 0.03f, 0.14f}, true},
        {0.1f, -0.03f, 0.38f, 72, 35, -1, -20, 0, {0.4f, 0.9f, 1}, {0.028f, 0.17f, 0.09f}, true},
        {0.1f, 0.03f, 0.34f, 66, 30, 1, -20, 0, {0.4f, 0.9f, 1}, {0.028f, 0.17f, 0.09f}, false}}}},
    // A low rosette of fat pointed leaves, flatter as it dries.
    {"succulent", "succulent echeveria haworthia crassula aloe jade sedum", {0, 0, 0, false, {
        {0.12f, -0.06f, 0.3f, 20, 2, -1, 0, 0, {0.15f, 0.7f, 1}, {0.11f, 0.15f, 0.05f}, false},
        {0.12f, 0.06f, 0.3f, 20, 2, 1, 0, 0, {0.15f, 0.7f, 1}, {0.11f, 0.15f, 0.05f}, false},
        {0.12f, -0.02f, 0.3f, 66, 42, -1, 0, 0, {0.15f, 0.7f, 1}, {0.11f, 0.15f, 0.05f}, true},
        {0.12f, 0.02f, 0.3f, 62, 40, 1, 0, 0, {0.15f, 0.7f, 1}, {0.11f, 0.15f, 0.05f}, true}}}},
    // Two broad leaves low over the soil and a thin arching spike, its flower at the top.
    {"orchid", "orchid phalaenopsis dendrobium", {0.025f, 0.2f, 0.0f, false, {
        {0.1f, -0.02f, 0.38f, 30, 0, -1, -15, 0, {0.2f, 0.8f, 1}, {0.12f, 0.15f, 0.08f}, false},
        {0.1f, 0.02f, 0.36f, 26, -4, 1, -15, 0, {0.2f, 0.8f, 1}, {0.12f, 0.15f, 0.08f}, true},
        {},
        {}}}},
    // Long oval leaves held up on thin stalks from the soil, fanned out (Ctenanthe, the never never plant).
    {"never", "never ctenanthe", {0, 0, 0, false, {
        {0.1f, -0.04f, 0.48f, 38, 12, -1, -8, 0, {0.35f, 0.88f, 1}, {0.025f, 0.15f, 0.07f}, false},
        {0.1f, 0.04f, 0.46f, 36, 10, 1, -8, 0, {0.35f, 0.88f, 1}, {0.025f, 0.15f, 0.07f}, false},
        {0.1f, -0.01f, 0.56f, 76, 50, -1, -8, 0, {0.4f, 0.88f, 1}, {0.025f, 0.14f, 0.065f}, true},
        {0.1f, 0.01f, 0.52f, 66, 42, 1, -8, 0, {0.4f, 0.88f, 1}, {0.025f, 0.14f, 0.065f}, true}}}},
    // A stem with a crown of leaflets spread from its top like the spokes of an umbrella (Schefflera), no flower.
    {"umbrella", "umbrella schefflera heptapleurum", {0.04f, 0.1f, -1, false, {
        {0.95f, 0, 0.36f, -10, -50, 1, 0, 0, {0.12f, 0.85f, 1}, {0.022f, 0.13f, 0.06f}, false},
        {0.95f, 0, 0.36f, -10, -50, -1, 0, 0, {0.12f, 0.85f, 1}, {0.022f, 0.13f, 0.06f}, false},
        {0.95f, 0, 0.3f, 32, -15, 1, 0, 0, {0.12f, 0.85f, 1}, {0.022f, 0.13f, 0.06f}, true},
        {0.95f, 0, 0.3f, 32, -15, -1, 0, 0, {0.12f, 0.85f, 1}, {0.022f, 0.13f, 0.06f}, true}}}},
};
inline constexpr uint8_t SHAPE_COUNT = sizeof(SHAPES) / sizeof(SHAPES[0]);

// The kind of a species ("Monstera deliciosa") or of the option's id ("snake"): the first whose word is in it, any
// case. Anything else is the first, the leafy plant.
inline uint8_t shape_of(const char *text) {
  char low[64];
  size_t n = 0;
  for (; text && text[n] && n + 1 < sizeof(low); ++n)
    low[n] = static_cast<char>(text[n] >= 'A' && text[n] <= 'Z' ? text[n] + 32 : text[n]);
  low[n] = 0;
  if (!n) return 0;
  for (uint8_t i = 1; i < SHAPE_COUNT; ++i) {
    for (const char *w = SHAPES[i].words; *w;) {
      const size_t len = std::strcspn(w, " ");
      char word[24] = "";
      std::memcpy(word, w, len < sizeof(word) ? len : sizeof(word) - 1);
      if (*word && std::strstr(low, word)) return i;
      w += len;
      while (*w == ' ') ++w;
    }
  }
  return 0;
}

}  // namespace esphome::microplant
