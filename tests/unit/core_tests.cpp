#include "core/model.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace crosshair;
namespace {
int checks = 0;
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}
Binding slot(unsigned index, unsigned code) {
    return {{Device::keyboard, static_cast<std::uint16_t>(code), 0},
            0,
            true,
            Action::slot,
            static_cast<std::uint8_t>(index)};
}
} // namespace
int main() {
    auto p = initialProfile();
    check(!validate(p), "blank configuration without game is valid");
    check(p.bindings.empty(), "no assumed keyboard bindings");
    p.bindings = {slot(0, 2), slot(1, 3), slot(2, 4)};
    check(!validate(p), "three direct slots");
    State s;
    check(!s.visiblePreset(p), "waiting without target");
    check(!s.apply(p.bindings[0]), "background input ignored");
    s.setEligible(true);
    check(s.visiblePreset(p) == p.defaultPresetId, "focus starts at fallback");
    for (int i : {0, 2, 1, 0}) {
        check(s.apply(p.bindings[i]), "direct selection changes state");
        check(s.visiblePreset(p) == p.slots[i].presetId, "correct selected preset");
        check(!s.apply(p.bindings[i]), "repeated selection is idempotent");
    }
    check(matchBinding(p.bindings, p.bindings[0].key, shift) == 0, "Shift sprint tolerated");
    auto specific = slot(3, 2);
    specific.requiredModifiers = shift;
    p.bindings.push_back(specific);
    check(!validate(p), "more specific binding allowed");
    check(matchBinding(p.bindings, specific.key, shift) == 3, "specific binding wins");
    p.bindings.back().requiredModifiers = 0;
    check(validate(p).has_value(), "duplicate rejected");
    p.bindings.back().requiredModifiers = control;
    p.bindings[0].requiredModifiers = shift;
    check(validate(p).has_value(), "equal specificity overlap rejected");
    p.bindings.resize(3);
    p.bindings[0].requiredModifiers = 0;
    p.bindings[0].allowExtraModifiers = false;
    check(!matchBinding(p.bindings, p.bindings[0].key, shift), "exact modifiers honored");
    p.bindings[0].allowExtraModifiers = true;
    auto extended = p.bindings[0].key;
    extended.extended = 1;
    check(!matchBinding(p.bindings, extended, 0), "extended keys distinct");
    Binding hide{{Device::mouse, 4, 0}, 0, true, Action::hide, 0};
    check(s.apply(hide) && !s.visiblePreset(p), "hide is distinct from pause");
    check(!s.manualPaused, "hide leaves input active");
    check(s.apply(p.bindings[1]) && s.visiblePreset(p).has_value(), "slot restores after hide");
    s.setPaused(true);
    check(!s.visiblePreset(p) && !s.apply(p.bindings[0]), "pause hides and ignores slots");
    s.setPaused(false);
    check(!s.selectedSlot && s.visiblePreset(p) == p.defaultPresetId, "resume defaults");
    s.apply(p.bindings[1]);
    s.setEligible(false);
    check(!s.selectedSlot && !s.visiblePreset(p), "focus loss invalidates selection");
    s.setEligible(true);
    check(s.visiblePreset(p) == p.defaultPresetId, "return uses fallback");
    s.profileEnabled = false;
    check(!s.visiblePreset(p) && !s.apply(p.bindings[0]), "disabled profile priority");
    s.profileEnabled = true;
    s.backendState = BackendState::unavailable;
    check(!s.visiblePreset(p), "unavailable backend hidden");
    s.backendState = BackendState::ready;
    s.exiting = true;
    check(!s.visiblePreset(p), "exit priority");
    InputEdges edges;
    const auto key = p.bindings[0].key;
    check(edges.press(key), "first key down");
    check(!edges.press(key), "autorepeat ignored");
    edges.loseFocus();
    check(!edges.press(key), "held key blocked after focus loss");
    edges.release(key);
    check(edges.press(key), "new press after release accepted");
    edges.release(key);
    edges.block(key);
    check(!edges.press(key), "key held on entering foreground blocked");
    edges.release(key);
    check(edges.press(key), "release clears foreground quarantine");
    for (const auto& preset : starterPresets())
        check(!validate(preset), "starter preset valid");
    check(templatePresets().size() == 8, "one entry per crosshair type");
    for (const auto& preset : templatePresets())
        check(!validate(preset) && findPreset(preset.id), "template valid and loadable");
    auto cross = *findPreset("type-cross");
    cross.layers.push_back(Layer{});
    auto geometry = armGeometry(cross);
    check(geometry && geometry->horizontalLength == 6 && geometry->verticalLength == 6 &&
              geometry->horizontalGap == 4 && geometry->verticalGap == 4, "cross parameters read from existing layers");
    geometry->horizontalLength = 12;
    geometry->verticalLength = 9;
    setArmGeometry(cross, *geometry);
    check(cross.layers[0].x == -16 && cross.layers[0].endX == -4 &&
              cross.layers[2].y == -13 && cross.layers[2].endY == -4,
          "arm lengths change independently without stretching gap");
    geometry->horizontalGap = geometry->verticalGap = 0;
    setArmGeometry(cross, *geometry);
    check(cross.layers[0].endX == 0 && cross.layers[1].x == 0 &&
              cross.layers[2].endY == 0 && cross.layers[3].y == 0 && armGeometry(cross),
          "zero gap joins all four arms and remains editable");
    geometry->horizontalGap = geometry->verticalGap = 7;
    setArmGeometry(cross, *geometry);
    check(cross.layers[0].x == -19 && cross.layers[0].endX == -7 &&
              cross.layers[1].x == 7 && cross.layers[1].endX == 19 &&
              cross.layers[2].y == -16 && cross.layers[3].endY == 16 && !validate(cross),
          "reopening gap preserves horizontal and vertical lengths");
    check(cross.layers.back().radius == 2 && cross.layers.back().x == 0 &&
              cross.layers[0].thickness == 2 && cross.layers[0].outline == 1 &&
              cross.layers[0].color.g == 1, "cross editing preserves dot and style");
    std::swap(cross.layers[0].x, cross.layers[0].endX);
    geometry->horizontalGap = geometry->verticalGap = 3;
    setArmGeometry(cross, *geometry);
    check(cross.layers[0].x == -3 && cross.layers[0].endX == -15,
          "reversed endpoints keep their direction");
    cross.layers[0].rotation = 45;
    check(!armGeometry(cross), "custom rotated lines retain generic editing");
    for (const auto& preset : templatePresets())
        check(armGeometry(preset).has_value() == (preset.id == "type-cross" || preset.id == "type-t" || preset.id == "type-ring-cross"),
              "arm controls cover cross, T and ring with cross");
    auto legacy = *findPreset("type-cross");
    legacy.layers = {{Shape::line, -8, 0, 8, 0}, {Shape::line, 0, -12, 0, 12}, Layer{}};
    auto arms = armGeometry(legacy);
    check(arms && arms->horizontalLength == 8 && arms->verticalLength == 12 && arms->horizontalGap == 0,
          "legacy continuous cross exposes arm geometry");
    arms->horizontalGap = 3;
    arms->verticalGap = 5;
    setArmGeometry(legacy, *arms);
    check(legacy.layers.size() == 5 && legacy.layers[2].shape == Shape::dot &&
              legacy.layers[0].x == -11 && legacy.layers[0].endX == -3 &&
              legacy.layers[1].y == -17 && legacy.layers[1].endY == -5,
          "legacy cross splits without losing style, lengths or existing layer indices");
    for (const auto id : {"type-t", "type-ring-cross"}) {
        auto preset = *findPreset(id);
        auto g = *armGeometry(preset);
        const auto count = preset.layers.size();
        const float radius = preset.layers.front().radius;
        g.horizontalLength = 15;
        setArmGeometry(preset, g);
        check(armGeometry(preset)->verticalLength == g.verticalLength &&
                  armGeometry(preset)->horizontalGap == g.horizontalGap && preset.layers.size() == count &&
                  preset.layers.front().radius == radius, "T and ring arms resize without adding arms or resizing ring");
        g.horizontalGap = g.verticalGap = 0;
        setArmGeometry(preset, g);
        check(armGeometry(preset) && !validate(preset), "T and ring cross support joined arms");
    }
    auto chevron = *findPreset("type-chevron");
    auto chevronParameters = *chevronGeometry(chevron);
    const auto initialAngle = chevronParameters.angle;
    chevronParameters.length = 20;
    chevronParameters.gap = 5;
    setChevronGeometry(chevron, chevronParameters);
    check(std::abs(chevronGeometry(chevron)->length - 20) < .001f &&
              std::abs(chevronGeometry(chevron)->angle - initialAngle) < .001f &&
              chevron.layers[0].endX == -5 && chevron.layers[1].x == 5,
          "chevron opens without stretching angle or length");
    chevronParameters.gap = 0;
    chevronParameters.angle = 30;
    setChevronGeometry(chevron, chevronParameters);
    check(chevron.layers[0].endX == 0 && chevron.layers[1].x == 0 &&
              std::abs(chevronGeometry(chevron)->length - 20) < .001f,
          "chevron rejoins and changes angle without changing length");
    auto corners = *findPreset("type-corners");
    auto cornerParameters = *cornerGeometry(corners);
    cornerParameters.horizontalLength = 12;
    cornerParameters.verticalLength = 9;
    setCornerGeometry(corners, cornerParameters);
    check(corners.layers[0].x == -4 && corners.layers[0].endX == -16 &&
              corners.layers[0].y == -13 && corners.layers[1].endY == -4,
          "corner arms keep independent lengths and openings with connected elbows");
    cornerParameters.horizontalGap = cornerParameters.verticalGap = 0;
    setCornerGeometry(corners, cornerParameters);
    check(cornerGeometry(corners) && corners.layers[0].x == 0 && corners.layers[1].endY == 0,
          "corners join into a closed outline and remain editable");
    cornerParameters.horizontalGap = 3;
    cornerParameters.verticalGap = 7;
    setCornerGeometry(corners, cornerParameters);
    check(cornerGeometry(corners)->horizontalLength == 12 && cornerGeometry(corners)->verticalLength == 9 &&
              cornerGeometry(corners)->horizontalGap == 3 && cornerGeometry(corners)->verticalGap == 7,
          "corners reopen independently without resizing arms");
    auto dots = *findPreset("type-dots");
    dots.layers.push_back(Layer{});
    setDotSpacing(dots, 20);
    check(std::abs(dotSpacing(dots) - 20) < .001f && dots.layers[0].radius == 1 &&
              dots.layers.back().radius == 2 && dots.layers.back().x == 0,
          "constellation spacing preserves point sizes and center dot");
    setDotSpacing(dots, .5f);
    setDotSpacing(dots, 12);
    check(std::abs(dotSpacing(dots) - 12) < .001f && dots.layers[0].x < 0 && dots.layers[0].y < 0,
          "near-touching constellation can reopen with directions intact");
    Layer diagonal{Shape::line, 2, 3, 5, 7};
    setLineLength(diagonal, 10);
    check(diagonal.x == .5f && diagonal.y == 1 && diagonal.endX == 6.5f && diagonal.endY == 9,
          "custom line length preserves direction and center position");
    auto bad = starterPresets()[0];
    bad.layers[0].radius = std::numeric_limits<float>::quiet_NaN();
    check(validate(bad).has_value(), "NaN geometry rejected");
    p.bindings[0].requiredModifiers = windows;
    check(validate(p).has_value(), "Windows shortcuts rejected");
    p.bindings[0] = {{Device::mouse, 5, 0}, 0, true, Action::slot, 0};
    check(!validate(p), "Mouse 5 supported");
    p.bindings[0].key.code = 6;
    check(validate(p).has_value(), "unverified HID buttons rejected");
    p.bindings[0] = slot(5, 2);
    check(validate(p).has_value(), "slot bounds checked");
    std::cout << checks << " core checks passed\n";
}
