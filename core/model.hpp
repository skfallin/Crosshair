#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace crosshair {
enum class Device : std::uint8_t { keyboard, mouse };
enum Modifier : std::uint8_t { shift = 1, control = 2, alt = 4, windows = 8 };
struct Key {
    Device device = Device::keyboard;
    std::uint16_t code = 0;    // Keyboard scan code, or mouse button 3, 4, 5.
    std::uint8_t extended = 0; // 0: normal, 1: E0, 2: E1.
    auto operator<=>(const Key&) const = default;
};
enum class Action : std::uint8_t { slot, hide, pause };
struct Binding {
    Key key;
    std::uint8_t requiredModifiers = 0;
    bool allowExtraModifiers = true;
    Action action = Action::slot;
    std::uint8_t slot = 0;
};
struct Slot {
    std::string label;
    std::string presetId;
};
enum class Shape : std::uint8_t { dot, line, ring, png };
struct Color {
    float r = 1, g = 1, b = 1, a = 1;
};
struct Layer {
    Shape shape = Shape::dot;
    float x = 0, y = 0;
    float endX = 0, endY = 0;
    float radius = 2, thickness = 1, outline = 1;
    Color color;
    float rotation = 0;
    std::string assetId;
    float width = 32, height = 32;
    bool nearest = true;
};
struct Preset {
    std::string id, name;
    std::vector<Layer> layers;
    std::string family;
    std::vector<std::string> tags;
    std::string provenance = "non specificata", author, source, license, permission;
};
struct Profile {
    std::string id = "generic";
    std::string name = "Generico";
    bool enabled = true;
    std::wstring executablePath;
    std::wstring windowClass;
    std::array<Slot, 5> slots;
    std::string defaultPresetId = "original-dot";
    int offsetX = 0, offsetY = 0;
    std::vector<Binding> bindings;
};
enum class BackendState { ready, unavailable };
struct State {
    bool profileEnabled = true;
    bool targetEligible = false;
    bool manualPaused = false;
    std::optional<std::uint8_t> selectedSlot;
    bool slotRequestsHidden = false;
    std::string defaultPresetId = "original-dot";
    BackendState backendState = BackendState::ready;
    bool previewMode = false;
    bool exiting = false;

    void setEligible(bool eligible);
    void setPaused(bool paused);
    bool apply(const Binding& binding);
    std::optional<std::string> visiblePreset(const Profile& profile) const;
};

// Only edges reach the reducer. On focus loss, already held keys stay blocked
// until release, so a repeat cannot reactivate a slot on returning to the game.
class InputEdges {
  public:
    bool press(Key key);
    void release(Key key);
    void loseFocus();
    void block(Key key);

  private:
    std::vector<Key> held_, blocked_;
};

bool matches(const Binding& binding, Key key, std::uint8_t modifiers);
bool validId(std::string_view id);
std::optional<std::size_t> matchBinding(std::span<const Binding> bindings, Key key, std::uint8_t modifiers);
std::optional<std::string> validate(const Profile& profile);
std::optional<std::string> validate(const Preset& preset);
const std::vector<Preset>& starterPresets();
const std::vector<Preset>& templatePresets();
struct ArmGeometry {
    float horizontalLength = 0, verticalLength = 0;
    float horizontalGap = 0, verticalGap = 0;
};
std::optional<ArmGeometry> armGeometry(const Preset& preset);
void setArmGeometry(Preset& preset, const ArmGeometry& geometry);
struct ChevronGeometry {
    float length, angle, gap;
};
std::optional<ChevronGeometry> chevronGeometry(const Preset& preset);
void setChevronGeometry(Preset& preset, const ChevronGeometry& geometry);
struct CornerGeometry {
    float horizontalLength, verticalLength, horizontalGap, verticalGap;
};
std::optional<CornerGeometry> cornerGeometry(const Preset& preset);
void setCornerGeometry(Preset& preset, const CornerGeometry& geometry);
float dotSpacing(const Preset& preset);
void setDotSpacing(Preset& preset, float distance);
void setLineLength(Layer& layer, float length);
const Preset* findPreset(std::string_view id);
Profile initialProfile();
} // namespace crosshair
