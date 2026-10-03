#include "core/model.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace crosshair {
namespace {
unsigned specificity(const Binding& binding) {
    return std::popcount(static_cast<unsigned>(binding.requiredModifiers));
}
bool finiteRange(float value, float low, float high) {
    return std::isfinite(value) && value >= low && value <= high;
}
} // namespace

bool validId(std::string_view id) {
    // IDs are appended to fixed directories; a prefix also avoids Windows device names.
    return !id.empty() && id.size() <= 80 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
bool matches(const Binding& b, Key key, std::uint8_t modifiers) {
    return b.key == key && (modifiers & b.requiredModifiers) == b.requiredModifiers &&
           (b.allowExtraModifiers || modifiers == b.requiredModifiers);
}
std::optional<std::size_t> matchBinding(std::span<const Binding> bindings, Key key, std::uint8_t modifiers) {
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (matches(bindings[i], key, modifiers) &&
            (!best || specificity(bindings[i]) > specificity(bindings[*best])))
            best = i;
    }
    return best;
}
std::optional<std::string> validate(const Profile& p) {
    if (!validId(p.id) || p.name.empty() || p.name.size() > 120)
        return "Identificatore o nome del profilo non valido.";
    if (p.executablePath.size() > 32767 || p.windowClass.size() > 256)
        return "Identità del bersaglio troppo lunga.";
    if (p.offsetX < -8192 || p.offsetX > 8192 || p.offsetY < -8192 || p.offsetY > 8192)
        return "Gli offset devono essere compresi tra -8192 e 8192 pixel.";
    if (!validId(p.defaultPresetId) || p.bindings.size() > 7)
        return "Preset predefinito o numero di associazioni non valido.";
    for (const auto& slot : p.slots)
        if (slot.label.size() > 120 || !validId(slot.presetId))
            return "Etichetta o preset dello slot non valido.";
    for (std::size_t i = 0; i < p.bindings.size(); ++i) {
        const auto& b = p.bindings[i];
        if ((b.requiredModifiers & ~15) != 0 || b.key.extended > 2 ||
            (b.key.device != Device::keyboard && b.key.device != Device::mouse) ||
            (b.action != Action::slot && b.action != Action::hide && b.action != Action::pause) ||
            (b.key.device == Device::keyboard && (b.key.code == 0 || b.key.code > 0x7f)) ||
            (b.key.device == Device::mouse && (b.key.code < 3 || b.key.code > 5 || b.key.extended)) ||
            (b.action == Action::slot && b.slot >= p.slots.size()))
            return "Tasto o destinazione non valido.";
        if (b.key.device == Device::keyboard) {
            // Modifiers alone are not slot keys; system shortcuts are not captured.
            if (b.key.code == 0x2a || b.key.code == 0x36 || b.key.code == 0x1d || b.key.code == 0x38 ||
                (b.key.extended == 1 && (b.key.code == 0x5b || b.key.code == 0x5c)))
                return "Scegli un tasto diverso dai soli modificatori.";
            if ((b.requiredModifiers & windows) ||
                ((b.requiredModifiers & alt) &&
                 (b.key.code == 0x0f || b.key.code == 0x3e || b.key.code == 0x01 || b.key.code == 0x39)) ||
                ((b.requiredModifiers & control) && b.key.code == 0x01) ||
                ((b.requiredModifiers & (control | alt)) == (control | alt) && b.key.code == 0x53 &&
                 b.key.extended == 1))
                return "Combinazione riservata a Windows: scegli un altro tasto.";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (b.action == p.bindings[j].action &&
                (b.action != Action::slot || b.slot == p.bindings[j].slot))
                return "È ammessa una sola associazione per azione.";
            for (std::uint8_t modifiers = 0; modifiers < 16; ++modifiers)
                if (specificity(b) == specificity(p.bindings[j]) && matches(b, b.key, modifiers) &&
                    matches(p.bindings[j], b.key, modifiers))
                    return "Conflitto fra associazioni: scegli tasti o modificatori diversi.";
        }
    }
    return {};
}
std::optional<std::string> validate(const Preset& p) {
    for (const auto* text : {&p.provenance, &p.author, &p.source, &p.license, &p.permission})
        if (text->size() > 1024)
            return "Metadati di provenienza oltre 1.024 byte.";
    if (!validId(p.id) || p.name.empty() || p.name.size() > 120 || p.layers.empty() || p.layers.size() > 16)
        return "Preset non valido.";
    for (const auto& l : p.layers) {
        if ((l.shape != Shape::dot && l.shape != Shape::line && l.shape != Shape::ring &&
             l.shape != Shape::png) ||
            !finiteRange(l.x, -96, 96) || !finiteRange(l.y, -96, 96) || !finiteRange(l.endX, -96, 96) ||
            !finiteRange(l.endY, -96, 96) || !finiteRange(l.radius, 0.5f, 96) ||
            !finiteRange(l.thickness, 0.5f, 16) || !finiteRange(l.outline, 0, 8) ||
            !finiteRange(l.color.r, 0, 1) || !finiteRange(l.color.g, 0, 1) || !finiteRange(l.color.b, 0, 1) ||
            !finiteRange(l.color.a, 0, 1) || !finiteRange(l.rotation, -360, 360))
            return "Geometria o colore fuori limite.";
        if (l.shape == Shape::png &&
            (!validId(l.assetId) || !finiteRange(l.width, .5f, 192) || !finiteRange(l.height, .5f, 192)))
            return "Livello PNG non valido.";
        const auto radians = l.rotation * 3.14159265358979323846f / 180;
        const auto c = std::cos(radians), s = std::sin(radians);
        const auto extentX = l.shape == Shape::png ? (std::abs(c) * l.width + std::abs(s) * l.height) / 2 : 0;
        const auto extentY = l.shape == Shape::png ? (std::abs(s) * l.width + std::abs(c) * l.height) / 2 : 0;
        const auto rotatedX = c * l.x - s * l.y, rotatedY = s * l.x + c * l.y;
        if (l.shape == Shape::png) {
            if (std::abs(rotatedX) + extentX > 127 || std::abs(rotatedY) + extentY > 127)
                return "Il PNG supera la superficie del mirino.";
            continue;
        }
        const float extent = l.shape == Shape::line
                                 ? l.thickness / 2 + l.outline
                                 : l.radius + l.outline + (l.shape == Shape::ring ? l.thickness / 2 : 0);
        if (std::abs(rotatedX) + extent > 127 || std::abs(rotatedY) + extent > 127 ||
            (l.shape == Shape::line && (std::abs(c * l.endX - s * l.endY) + extent > 127 ||
                                        std::abs(s * l.endX + c * l.endY) + extent > 127)))
            return "La geometria supera la superficie del mirino.";
    }
    return {};
}

void State::setEligible(bool eligible) {
    if (targetEligible == eligible)
        return;
    targetEligible = eligible;
    selectedSlot.reset();
    slotRequestsHidden = false;
}
void State::setPaused(bool paused) {
    if (manualPaused == paused)
        return;
    manualPaused = paused;
    selectedSlot.reset();
    slotRequestsHidden = false;
}
bool State::apply(const Binding& b) {
    if (exiting || !profileEnabled || !(targetEligible || previewMode))
        return false;
    if (b.action == Action::pause) {
        setPaused(!manualPaused);
        return true;
    }
    if (manualPaused)
        return false;
    if (b.action == Action::hide) {
        const bool changed = !slotRequestsHidden;
        slotRequestsHidden = true;
        selectedSlot.reset();
        return changed;
    }
    if (b.slot >= 5)
        return false;
    const bool changed = selectedSlot != b.slot || slotRequestsHidden;
    selectedSlot = b.slot;
    slotRequestsHidden = false;
    return changed;
}
std::optional<std::string> State::visiblePreset(const Profile& p) const {
    if (exiting || !profileEnabled || !(targetEligible || previewMode) || manualPaused ||
        slotRequestsHidden || backendState != BackendState::ready)
        return {};
    if (selectedSlot && *selectedSlot < p.slots.size())
        return p.slots[*selectedSlot].presetId;
    return defaultPresetId;
}
bool InputEdges::press(Key key) {
    if (std::find(blocked_.begin(), blocked_.end(), key) != blocked_.end() ||
        std::find(held_.begin(), held_.end(), key) != held_.end())
        return false;
    held_.push_back(key);
    return true;
}
void InputEdges::release(Key key) {
    std::erase(held_, key);
    std::erase(blocked_, key);
}
void InputEdges::loseFocus() {
    for (const auto key : held_)
        block(key);
    held_.clear();
}
void InputEdges::block(Key key) {
    if (std::find(blocked_.begin(), blocked_.end(), key) == blocked_.end())
        blocked_.push_back(key);
}
const std::vector<Preset>& starterPresets() {
    static const auto presets = [] {
        std::vector<Preset> values{
            {"original-dot", "Punto", {{Shape::dot, 0, 0, 0, 0, 2, 1, 1, {1, 1, 1, 1}}}},
            {"original-cross",
             "Croce aperta",
             {{Shape::line, -10, 0, -4, 0, 2, 2, 1, {0, 1, 0.6f, 1}},
              {Shape::line, 4, 0, 10, 0, 2, 2, 1, {0, 1, 0.6f, 1}},
              {Shape::line, 0, -10, 0, -4, 2, 2, 1, {0, 1, 0.6f, 1}},
              {Shape::line, 0, 4, 0, 10, 2, 2, 1, {0, 1, 0.6f, 1}}}},
            {"original-ring",
             "Anello con punto",
             {{Shape::ring, 0, 0, 0, 0, 9, 2, 1, {1, 1, 1, 1}},
              {Shape::dot, 0, 0, 0, 0, 1, 1, 1, {1, 1, 1, 1}}}}};
        for (auto& p : values) {
            p.provenance = "geometria originale del progetto";
            p.source = "core/model.cpp";
        }
        return values;
    }();
    return presets;
}
const std::vector<Preset>& templatePresets() {
    static const auto presets = [] {
        const auto line = [](float x, float y, float ex, float ey) {
            return Layer{Shape::line, x, y, ex, ey, 2, 2, 1};
        };
        const Layer ring{Shape::ring, 0, 0, 0, 0, 9, 2, 1};
        std::vector<Preset> values{
            {"type-dot", "Punto", {Layer{}}},
            {"type-cross", "Croce", starterPresets()[1].layers},
            {"type-circle", "Cerchio", {ring}},
            {"type-t", "Forma a T", {line(-10, 0, -4, 0), line(4, 0, 10, 0), line(0, 4, 0, 10)}},
            {"type-chevron", "Chevron", {line(-10, 10, 0, 0), line(0, 0, 10, 10)}},
            {"type-corners", "Angoli", {}},
            {"type-ring-cross", "Cerchio con croce", {ring, line(-18, 0, -12, 0), line(12, 0, 18, 0),
                line(0, -18, 0, -12), line(0, 12, 0, 18)}},
            {"type-dots", "Costellazione", {}}
        };
        for (float x : {-1.f, 1.f})
            for (float y : {-1.f, 1.f}) {
                values[5].layers.push_back(line(x * 4, y * 10, x * 10, y * 10));
                values[5].layers.push_back(line(x * 10, y * 10, x * 10, y * 4));
                values[7].layers.push_back({Shape::dot, x * 6, y * 6, 0, 0, 1});
            }
        for (auto& p : values) {
            p.family = "tipi";
            p.provenance = "geometria originale del progetto";
            p.source = "core/model.cpp";
        }
        return values;
    }();
    return presets;
}
namespace {
bool near(float a, float b) { return std::abs(a - b) < .001f; }
bool centerShape(const Layer& l) {
    return (l.shape == Shape::dot || l.shape == Shape::ring) && l.x == 0 && l.y == 0;
}
std::vector<Layer> splitCenteredLines(const Preset& p) {
    auto layers = p.layers;
    for (std::size_t i = 0; i < p.layers.size(); ++i) {
        auto l = p.layers[i];
        if (l.shape == Shape::line && l.rotation == 0 &&
            ((l.y == 0 && l.endY == 0 && l.x * l.endX < 0) ||
             (l.x == 0 && l.endX == 0 && l.y * l.endY < 0))) {
            auto second = l;
            l.endX = l.endY = 0;
            second.x = second.y = 0;
            layers[i] = l;
            layers.push_back(second);
        }
    }
    return layers;
}
} // namespace
std::optional<ArmGeometry> armGeometry(const Preset& p) {
    std::array<float, 4> gaps{}, lengths{};
    std::array<bool, 4> found{};
    for (const auto& l : splitCenteredLines(p)) {
        if (centerShape(l))
            continue;
        if (l.shape != Shape::line || l.rotation != 0)
            return {};
        const bool vertical = l.x == 0 && l.endX == 0;
        if (!vertical && (l.y != 0 || l.endY != 0))
            return {};
        const float start = vertical ? l.y : l.x, end = vertical ? l.endY : l.endX;
        if (start == end)
            return {};
        const auto index = (vertical ? 2 : 0) + (start + end > 0 ? 1 : 0);
        if (found[index])
            return {};
        found[index] = true;
        gaps[index] = std::min(std::abs(start), std::abs(end));
        lengths[index] = std::abs(end - start);
    }
    if (std::none_of(found.begin(), found.end(), [](bool value) { return value; }))
        return {};
    for (int i : {0, 2})
        if (found[i] && found[i + 1] &&
            (!near(gaps[i], gaps[i + 1]) || !near(lengths[i], lengths[i + 1])))
            return {};
    const int horizontal = found[0] ? 0 : 1, vertical = found[2] ? 2 : 3;
    return ArmGeometry{lengths[horizontal], lengths[vertical], gaps[horizontal], gaps[vertical]};
}
void setArmGeometry(Preset& p, const ArmGeometry& geometry) {
    if (!armGeometry(p))
        return;
    p.layers = splitCenteredLines(p);
    for (auto& l : p.layers) {
        if (l.shape != Shape::line)
            continue;
        const bool vertical = l.x == 0 && l.endX == 0;
        auto& start = vertical ? l.y : l.x;
        auto& end = vertical ? l.endY : l.endX;
        const float sign = start + end < 0 ? -1.f : 1.f;
        const bool startIsInner = std::abs(start) < std::abs(end);
        const float inner = sign * (vertical ? geometry.verticalGap : geometry.horizontalGap);
        const float outer = inner + sign * (vertical ? geometry.verticalLength : geometry.horizontalLength);
        start = startIsInner ? inner : outer;
        end = startIsInner ? outer : inner;
    }
}
std::optional<ChevronGeometry> chevronGeometry(const Preset& p) {
    std::optional<ChevronGeometry> geometry;
    std::array<bool, 2> found{};
    float height = 0;
    for (const auto& l : p.layers) {
        if (centerShape(l))
            continue;
        if (l.shape != Shape::line || l.rotation != 0 || (l.y != 0 && l.endY != 0))
            return {};
        const bool startIsInner = l.y == 0;
        const float inner = startIsInner ? l.x : l.endX;
        const float outer = startIsInner ? l.endX : l.x;
        const float y = startIsInner ? l.endY : l.y;
        if (y == 0 || std::abs(outer) <= std::abs(inner) || inner * outer < 0)
            return {};
        const int side = outer > 0 ? 1 : 0;
        if (found[side])
            return {};
        found[side] = true;
        const float dx = std::abs(outer) - std::abs(inner);
        const ChevronGeometry current{std::hypot(dx, y),
            std::atan2(std::abs(y), dx) * 180 / 3.14159265358979323846f, std::abs(inner)};
        if (geometry && (!near(current.length, geometry->length) || !near(current.angle, geometry->angle) ||
                         !near(current.gap, geometry->gap) || !near(y, height)))
            return {};
        geometry = current;
        height = y;
    }
    return found[0] && found[1] ? geometry : std::nullopt;
}
void setChevronGeometry(Preset& p, const ChevronGeometry& geometry) {
    if (!chevronGeometry(p))
        return;
    const float angle = geometry.angle * 3.14159265358979323846f / 180;
    for (auto& l : p.layers) {
        if (l.shape != Shape::line)
            continue;
        const bool startIsInner = l.y == 0;
        const float signX = l.x + l.endX < 0 ? -1.f : 1.f;
        const float signY = l.y + l.endY < 0 ? -1.f : 1.f;
        const float inner = signX * geometry.gap;
        const float outer = signX * (geometry.gap + geometry.length * std::cos(angle));
        const float height = signY * geometry.length * std::sin(angle);
        l.x = startIsInner ? inner : outer;
        l.endX = startIsInner ? outer : inner;
        l.y = startIsInner ? 0 : height;
        l.endY = startIsInner ? height : 0;
    }
}
std::optional<CornerGeometry> cornerGeometry(const Preset& p) {
    std::array<bool, 8> found{};
    float outerX = 0, outerY = 0, gapX = 0, gapY = 0;
    bool horizontalFound = false, verticalFound = false;
    for (const auto& l : p.layers) {
        if (centerShape(l))
            continue;
        if (l.shape != Shape::line || l.rotation != 0)
            return {};
        const bool horizontal = l.y == l.endY && l.y != 0 && l.x != l.endX && l.x * l.endX >= 0;
        const bool vertical = l.x == l.endX && l.x != 0 && l.y != l.endY && l.y * l.endY >= 0;
        if (!horizontal && !vertical)
            return {};
        const int index = (l.x + l.endX > 0 ? 4 : 0) + (l.y + l.endY > 0 ? 2 : 0) + (vertical ? 1 : 0);
        if (found[index])
            return {};
        found[index] = true;
        const float x = std::max(std::abs(l.x), std::abs(l.endX));
        const float y = std::max(std::abs(l.y), std::abs(l.endY));
        if ((horizontalFound || verticalFound) && (!near(x, outerX) || !near(y, outerY)))
            return {};
        outerX = x;
        outerY = y;
        if (horizontal) {
            const float gap = std::min(std::abs(l.x), std::abs(l.endX));
            if (horizontalFound && !near(gap, gapX))
                return {};
            horizontalFound = true;
            gapX = gap;
        } else {
            const float gap = std::min(std::abs(l.y), std::abs(l.endY));
            if (verticalFound && !near(gap, gapY))
                return {};
            verticalFound = true;
            gapY = gap;
        }
    }
    if (!std::all_of(found.begin(), found.end(), [](bool value) { return value; }))
        return {};
    return CornerGeometry{outerX - gapX, outerY - gapY, gapX, gapY};
}
void setCornerGeometry(Preset& p, const CornerGeometry& geometry) {
    if (!cornerGeometry(p))
        return;
    for (auto& l : p.layers) {
        if (l.shape != Shape::line)
            continue;
        const float signX = l.x + l.endX < 0 ? -1.f : 1.f;
        const float signY = l.y + l.endY < 0 ? -1.f : 1.f;
        const float outerX = signX * (geometry.horizontalGap + geometry.horizontalLength);
        const float outerY = signY * (geometry.verticalGap + geometry.verticalLength);
        if (l.y == l.endY) {
            const bool startIsInner = std::abs(l.x) < std::abs(l.endX);
            l.x = startIsInner ? signX * geometry.horizontalGap : outerX;
            l.endX = startIsInner ? outerX : signX * geometry.horizontalGap;
            l.y = l.endY = outerY;
        } else {
            const bool startIsInner = std::abs(l.y) < std::abs(l.endY);
            l.y = startIsInner ? signY * geometry.verticalGap : outerY;
            l.endY = startIsInner ? outerY : signY * geometry.verticalGap;
            l.x = l.endX = outerX;
        }
    }
}
float dotSpacing(const Preset& p) {
    float distance = 0;
    for (const auto& l : p.layers)
        if (l.shape == Shape::dot)
            distance = std::max(distance, std::hypot(l.x, l.y));
    return distance;
}
void setDotSpacing(Preset& p, float distance) {
    const float previous = dotSpacing(p);
    if (previous <= 0 || distance <= 0)
        return;
    for (auto& l : p.layers)
        if (l.shape == Shape::dot) {
            l.x *= distance / previous;
            l.y *= distance / previous;
        }
}
void setLineLength(Layer& l, float length) {
    const float previous = std::hypot(l.endX - l.x, l.endY - l.y);
    if (l.shape != Shape::line || previous == 0)
        return;
    const float dx = (l.endX - l.x) * length / previous;
    const float dy = (l.endY - l.y) * length / previous;
    const float x = (l.x + l.endX) / 2, y = (l.y + l.endY) / 2;
    l.x = x - dx / 2;
    l.y = y - dy / 2;
    l.endX = x + dx / 2;
    l.endY = y + dy / 2;
}
const Preset* findPreset(std::string_view id) {
    for (const auto& p : templatePresets())
        if (p.id == id)
            return &p;
    for (const auto& p : starterPresets())
        if (p.id == id)
            return &p;
    return nullptr;
}
Profile initialProfile() {
    Profile result;
    for (std::size_t i = 0; i < result.slots.size(); ++i)
        result.slots[i] = {"Slot " + std::to_string(i + 1), starterPresets()[i % 3].id};
    return result; // No assumptions about the user's keys or weapon names.
}
} // namespace crosshair
