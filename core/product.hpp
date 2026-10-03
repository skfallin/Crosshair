#pragma once
#include <string_view>

namespace crosshair::product {
inline constexpr std::wstring_view name = L"Crosshair Native";
inline constexpr std::wstring_view directory = L"CrosshairNative";
inline constexpr std::wstring_view engineClass = L"CrosshairNative.Engine.Window.v1";
inline constexpr std::wstring_view overlayClass = L"CrosshairNative.Overlay.Window.v1";
inline constexpr std::wstring_view targetClass = L"CrosshairNative.TestTarget.Window.v1";
inline constexpr int schemaVersion = 1;
} // namespace crosshair::product
