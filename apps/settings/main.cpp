#include <windows.h>
#include <shobjidl.h>
#include <psapi.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Interop.h>
#include "apps/assets/resources.h"
#include "apps/ipc.hpp"
#include "apps/engine/storage.hpp"
#include "core/product.hpp"
#include "resources.h"
#include "design.hpp"
#include "core/preset_json.hpp"
#include "core/pack.hpp"
#include "rendering/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
#include <filesystem>
#include <functional>
#include <deque>
#include <map>
#include <memory>
#include <limits>
#include <cctype>
#include <cwctype>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Foundation;
using namespace crosshair;
namespace {
bool smokeTest = false;
bool soakTest = false;
bool designPreview = false;
std::string newId() {
    GUID id{};
    check_hresult(CoCreateGuid(&id));
    wchar_t value[40]{};
    StringFromGUID2(id, value, 40);
    auto result = to_string(hstring(value + 1, 36));
    std::transform(result.begin(), result.end(), result.begin(),
                   [](char c) { return c >= 'A' && c <= 'F' ? static_cast<char>(c - 'A' + 'a') : c; });
    return result;
}
hstring text(UINT id) {
    wchar_t buffer[2048]{};
    LoadStringW(GetModuleHandleW(nullptr), id, buffer, 2048);
    return buffer;
}
TextBlock label(hstring value, double size = 14) {
    TextBlock result;
    result.Text(value);
    result.FontSize(size);
    result.TextWrapping(TextWrapping::Wrap);
    if (size >= 20)
        result.FontWeight(winrt::Windows::UI::Text::FontWeight{700});
    return result;
}
Style style(hstring key) {
    return Application::Current().Resources().Lookup(box_value(key)).as<Style>();
}
TextBlock muted(hstring value, double size = 14) {
    auto result = label(value, size);
    result.Style(style(L"MutedTextStyle"));
    return result;
}
Border panel(UIElement const& content, double padding = 20) {
    Border result;
    result.Style(style(L"PanelStyle"));
    result.Padding({padding, padding, padding, padding});
    result.Child(content);
    return result;
}
void name(DependencyObject const& control, hstring value) {
    Automation::AutomationProperties::SetName(control, value);
}
StackPanel stack(double gap = 12) {
    StackPanel result;
    result.Spacing(gap);
    return result;
}
Grid wrapControls(StackPanel const& controls, int columns = 3, double minimum = 180) {
    Grid grid;
    grid.ColumnSpacing(8);
    grid.RowSpacing(8);
    for (int i = 0; i < columns; ++i) {
        ColumnDefinition column;
        column.Width({1, GridUnitType::Star});
        grid.ColumnDefinitions().Append(column);
    }
    int index = 0;
    while (controls.Children().Size()) {
        const auto child = controls.Children().GetAt(0);
        controls.Children().RemoveAt(0);
        if (index % columns == 0) {
            RowDefinition row;
            row.Height({1, GridUnitType::Auto});
            grid.RowDefinitions().Append(row);
        }
        Grid::SetColumn(child.as<FrameworkElement>(), index % columns);
        Grid::SetRow(child.as<FrameworkElement>(), index / columns);
        grid.Children().Append(child);
        ++index;
    }
    grid.SizeChanged([columns, minimum](auto const& sender, auto const& args) {
        const auto grid = sender.template as<Grid>();
        const int count =
            std::clamp(static_cast<int>((args.NewSize().Width + 8) / (minimum + 8)), 1, columns);
        if (grid.ColumnDefinitions().Size() == static_cast<unsigned>(count))
            return;
        grid.ColumnDefinitions().Clear();
        grid.RowDefinitions().Clear();
        for (int i = 0; i < count; ++i) {
            ColumnDefinition column;
            column.Width({1, GridUnitType::Star});
            grid.ColumnDefinitions().Append(column);
        }
        for (unsigned i = 0; i < grid.Children().Size(); ++i) {
            if (i % count == 0) {
                RowDefinition row;
                row.Height({1, GridUnitType::Auto});
                grid.RowDefinitions().Append(row);
            }
            const auto child = grid.Children().GetAt(i).as<FrameworkElement>();
            Grid::SetColumn(child, i % count);
            Grid::SetRow(child, i / count);
        }
    });
    return grid;
}
Button button(hstring value, std::function<void()> action) {
    Button result;
    result.Content(label(value));
    name(result, value);
    result.Click([action = std::move(action)](auto&&, auto&&) { action(); });
    return result;
}
Grid withInspector(UIElement const& primary, UIElement const& inspector) {
    Grid grid;
    grid.ColumnSpacing(24);
    grid.RowSpacing(24);
    ColumnDefinition main, side;
    main.Width({1, GridUnitType::Star});
    side.Width({300, GridUnitType::Pixel});
    grid.ColumnDefinitions().Append(main);
    grid.ColumnDefinitions().Append(side);
    for (int i = 0; i < 2; ++i) {
        RowDefinition row;
        row.Height({1, GridUnitType::Auto});
        grid.RowDefinitions().Append(row);
    }
    Grid::SetColumn(inspector.as<FrameworkElement>(), 1);
    grid.Children().Append(primary);
    grid.Children().Append(inspector);
    grid.SizeChanged([side](auto const& sender, auto const& args) {
        const auto grid = sender.template as<Grid>();
        const auto main = grid.Children().GetAt(0).as<FrameworkElement>();
        const auto inspector = grid.Children().GetAt(1).as<FrameworkElement>();
        const bool narrow = args.NewSize().Width < 700;
        side.Width({narrow ? 0.0 : 300.0, GridUnitType::Pixel});
        Grid::SetColumnSpan(main, narrow ? 2 : 1);
        Grid::SetColumn(inspector, narrow ? 0 : 1);
        Grid::SetRow(inspector, narrow ? 1 : 0);
        Grid::SetColumnSpan(inspector, narrow ? 2 : 1);
    });
    return grid;
}
void launchSibling(const wchar_t* filename, std::wstring_view arguments = L"") {
    wchar_t module[32768]{};
    GetModuleFileNameW(nullptr, module, 32768);
    const auto file = (std::filesystem::path(module).parent_path() / filename).wstring();
    auto command = L"\"" + file + L"\" " + std::wstring(arguments);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &process))
        throw std::runtime_error("Eseguibile non disponibile. Ricompila la distribuzione completa.");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
std::optional<std::filesystem::path> chooseFile(HWND owner, bool save, const wchar_t* title,
                                                const wchar_t* extension, const wchar_t* filter) {
    com_ptr<IFileDialog> dialog;
    check_hresult(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put())));
    check_hresult(dialog->SetTitle(title));
    COMDLG_FILTERSPEC type{title, filter};
    check_hresult(dialog->SetFileTypes(1, &type));
    check_hresult(dialog->SetDefaultExtension(extension));
    DWORD options = 0;
    check_hresult(dialog->GetOptions(&options));
    check_hresult(
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST)));
    const auto result = dialog->Show(owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {};
    check_hresult(result);
    com_ptr<IShellItem> item;
    check_hresult(dialog->GetResult(item.put()));
    PWSTR path = nullptr;
    check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH, &path));
    std::filesystem::path file(path);
    CoTaskMemFree(path);
    return file;
}
struct SettingsApp : ApplicationT<SettingsApp, Markup::IXamlMetadataProvider> {
    XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata{nullptr};
    Markup::IXamlType GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) {
        return metadata.GetXamlType(type);
    }
    Markup::IXamlType GetXamlType(hstring const& type) {
        return metadata.GetXamlType(type);
    }
    com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() {
        return metadata.GetXmlnsDefinitions();
    }
    Window window{nullptr};
    ContentControl interactionHost{nullptr};
    StackPanel root{nullptr}, page{nullptr}, toolbar{nullptr};
    std::array<Primitives::ToggleButton, 5> navigation{nullptr, nullptr, nullptr, nullptr, nullptr};
    TextBlock pageTitle{nullptr}, pageSubtitle{nullptr};
    ComboBox profileSelector{nullptr};
    GridView library{nullptr};
    StackPanel libraryDetails{nullptr};
    std::vector<Preset> catalog;
    std::map<std::string, Media::Imaging::WriteableBitmap> thumbnails;
    std::unique_ptr<Renderer> previewRenderer;
    std::string search, selectedPreset = "type-dot";
    std::vector<std::string> favorites, recents;
    bool personalLibrary = false, filteringLibrary = false;
    TextBlock libraryEmpty{nullptr};
    bool onlyFavorites = false, onlyRecent = false, autoStart = false, diagnostics = false, writable = true;
    Preset editing = templatePresets().front();
    std::vector<Preset> undo, redo;
    std::shared_ptr<Pack> importJob;
    std::size_t importAssetIndex = 0, importPresetIndex = 0;
    int selectedLayer = 0;
    bool editorDirty = false, lightPreview = false, advancedLayers = false;
    float crossGap = 4, crossLength = 8;
    Image editorPreview{nullptr};
    TextBlock status{nullptr}, feedback{nullptr};
    DispatcherTimer timer{nullptr};
    Profile draft = initialProfile();
    std::vector<JsonObject> targets;
    std::vector<JsonObject> profiles;
    std::deque<std::function<void()>> pending;
    std::array<hstring, 7> bindingNames{};
    bool dirty = false, busy = false, loaded = false, closing = false, closed = false, paused = false;
    int section = 1, guide = -1, capturing = -1;
    double configRevision = 0, captureId = 0;
    unsigned nextId = 1;
    unsigned editRevision = 0;
    HWND handle = nullptr;

    SettingsApp() {
        metadata = XamlTypeInfo::XamlControlsXamlMetaDataProvider();
        UnhandledException([](auto&&, UnhandledExceptionEventArgs const& e) {
            std::fprintf(stderr, "WinUI: %s\n", to_string(e.Message()).c_str());
        });
    }
    void OnLaunched(LaunchActivatedEventArgs const&) {
        // The composable Application and metadata provider must be fully
        // constructed before WinUI resolves its control resource dictionary.
        Resources().MergedDictionaries().Append(XamlControlsResources());
        Resources().MergedDictionaries().Append(
            Markup::XamlReader::Load(designResources).as<ResourceDictionary>());
        previewRenderer = std::make_unique<Renderer>();
        loadCatalog();
        window = Window();
        window.Title(product::name);
        window.as<::IWindowNative>()->get_WindowHandle(&handle);
        const auto appIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_CROSSHAIR));
        if (!appIcon)
            throw_last_error();
        window.AppWindow().SetIcon(winrt::Microsoft::UI::GetIconIdFromIcon(appIcon));
        SetPropW(handle, L"CrosshairNative.Settings", reinterpret_cast<HANDLE>(1));
        root = stack(24);
        root.Margin({32, 28, 32, 0});
        auto profileRow = stack(8);
        profileRow.Orientation(Orientation::Horizontal);
        profileSelector = ComboBox();
        profileSelector.PlaceholderText(text(IDS_SAVED_PROFILES));
        name(profileSelector, text(IDS_SAVED_PROFILES));
        profileSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
        profileSelector.SelectionChanged([this](auto&&, auto&&) {
            const auto index = profileSelector.SelectedIndex();
            if (index < 0 || static_cast<std::size_t>(index) >= profiles.size())
                return;
            const auto id = profiles[index].GetNamedString(L"id");
            if (to_string(id) == draft.id)
                return;
            if (dirty) {
                feedback.Text(text(IDS_SAVE_FIRST));
                return;
            }
            JsonObject p;
            p.Insert(L"profileId", JsonValue::CreateStringValue(id));
            p.Insert(L"configRevision", JsonValue::CreateNumberValue(configRevision));
            send(L"SwitchProfile", p, [this](JsonObject result) {
                draft = decodeProfile(to_string(result.GetNamedObject(L"profile").Stringify()));
                configRevision = result.GetNamedNumber(L"configRevision");
                bindingNames = {};
                render();
            });
        });
        profileRow.Children().Append(profileSelector);
        profileRow.Children().Append(button(text(IDS_NEW_PROFILE), [this] { newProfile(false); }));
        profileRow.Children().Append(button(text(IDS_DUPLICATE_PROFILE), [this] { newProfile(true); }));
        Grid profileBar;
        profileBar.ColumnSpacing(8);
        for (int i = 0; i < 3; ++i) {
            ColumnDefinition column;
            column.Width({1, i == 0 ? GridUnitType::Star : GridUnitType::Auto});
            profileBar.ColumnDefinitions().Append(column);
            auto child = profileRow.Children().GetAt(0).as<FrameworkElement>();
            profileRow.Children().RemoveAt(0);
            Grid::SetColumn(child, i);
            profileBar.Children().Append(child);
        }
        profileSelector.MaxWidth(320);
        profileSelector.HorizontalAlignment(HorizontalAlignment::Left);
        root.Children().Append(profileBar);
        auto heading = stack(6);
        pageTitle = label(L"", 32);
        pageSubtitle = muted(L"");
        heading.Children().Append(pageTitle);
        heading.Children().Append(pageSubtitle);
        root.Children().Append(heading);
        toolbar = stack(6);
        int position = 0;
        for (const auto [id, index] : std::array<std::pair<UINT, int>, 5>{
                 {{IDS_BINDINGS, 1}, {IDS_LIBRARY, 0}, {IDS_EDITOR, 2}, {IDS_SETTINGS, 3}, {IDS_GUIDE, 4}}}) {
            Primitives::ToggleButton tab;
            tab.Style(style(L"NavigationStyle"));
            auto entry = stack(12);
            entry.Orientation(Orientation::Horizontal);
            const std::array<Symbol, 5> symbols{Symbol::AllApps, Symbol::Library, Symbol::Edit,
                                                Symbol::Setting, Symbol::Help};
            SymbolIcon icon(symbols[position]);
            icon.Width(18);
            entry.Children().Append(icon);
            entry.Children().Append(label(text(id)));
            tab.Content(entry);
            name(tab, text(id));
            tab.Click([this, index](auto&&, auto&&) {
                if (index == 4)
                    guide = 0;
                else {
                    section = index;
                    guide = -1;
                }
                render();
            });
            navigation[position++] = tab;
            toolbar.Children().Append(tab);
        }
        page = stack(20);
        page.Margin({32, 24, 32, 28});
        feedback = label(L"");
        feedback.Style(style(L"AquaTextStyle"));
        Automation::AutomationProperties::SetLiveSetting(feedback,
                                                         Automation::Peers::AutomationLiveSetting::Polite);
        auto actions = stack(8);
        actions.Orientation(Orientation::Horizontal);
        auto saveButton = button(text(IDS_SAVE), [this] { save(); });
        saveButton.Style(style(L"PrimaryButtonStyle"));
        saveButton.AccessKey(L"S");
        actions.Children().Append(saveButton);
        actions.Children().Append(button(text(IDS_RELOAD), [this] { reload(); }));
        actions.Children().Append(button(text(IDS_PAUSE), [this] {
            JsonObject p;
            p.Insert(L"paused", JsonValue::CreateBooleanValue(!paused));
            send(L"SetPaused", p, [this](auto&&) { poll(); });
        }));
        auto engineButton = button(text(IDS_ENGINE_START), [this] {
            try {
                launchSibling(L"CrosshairNative.Engine.exe");
            } catch (const std::exception& e) {
                error(e.what());
            }
        });
        auto footer = stack(8);
        footer.Children().Append(feedback);
        footer.Children().Append(wrapControls(actions, 3, 130));
        footer.Margin({32, 12, 32, 24});
        ScrollViewer scroll;
        scroll.Content(page);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        Grid shell;
        shell.Style(style(L"CanvasStyle"));
        ColumnDefinition railColumn, mainColumn;
        railColumn.Width({212, GridUnitType::Pixel});
        mainColumn.Width({1, GridUnitType::Star});
        shell.ColumnDefinitions().Append(railColumn);
        shell.ColumnDefinitions().Append(mainColumn);
        for (const auto length : {GridLength{1, GridUnitType::Auto}, GridLength{1, GridUnitType::Star},
                                  GridLength{1, GridUnitType::Auto}}) {
            RowDefinition row;
            row.Height(length);
            shell.RowDefinitions().Append(row);
        }
        Grid::SetRow(root, 0);
        Grid::SetRow(scroll, 1);
        Grid::SetRow(footer, 2);
        Grid::SetColumn(root, 1);
        Grid::SetColumn(scroll, 1);
        Grid::SetColumn(footer, 1);
        shell.Children().Append(root);
        shell.Children().Append(scroll);
        shell.Children().Append(footer);
        auto rail =
            Markup::XamlReader::Load(
                LR"(<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Background="{ThemeResource SidebarBrush}" Padding="20,32,20,24"><Grid.RowDefinitions><RowDefinition Height="Auto"/><RowDefinition Height="*"/><RowDefinition Height="Auto"/></Grid.RowDefinitions></Grid>)")
                .as<Grid>();
        auto brand = stack(4);
        auto mark =
            Markup::XamlReader::Load(
                LR"(<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Width="40" Height="40" HorizontalAlignment="Left" Background="{ThemeResource BrandBrush}" CornerRadius="10" Margin="0,0,0,16"><Path Data="M 20,9 L 20,16 M 20,24 L 20,31 M 9,20 L 16,20 M 24,20 L 31,20" Stroke="{ThemeResource OnBrandBrush}" StrokeThickness="2.5"/></Border>)")
                .as<Border>();
        brand.Children().Append(mark);
        brand.Children().Append(label(L"CROSSHAIR", 20));
        auto native = muted(L"N A T I V E", 12);
        brand.Children().Append(native);
        rail.Children().Append(brand);
        toolbar.Margin({0, 40, 0, 24});
        Grid::SetRow(toolbar, 1);
        rail.Children().Append(toolbar);
        auto engine = stack(12);
        engine.Children().Append(muted(text(IDS_ENGINE_STATUS), 12));
        status = label(text(IDS_WAITING));
        status.Style(style(L"AquaTextStyle"));
        engine.Children().Append(status);
        engine.Children().Append(engineButton);
        Grid::SetRow(engine, 2);
        rail.Children().Append(engine);
        Grid::SetRowSpan(rail, 3);
        shell.Children().Append(rail);
        shell.SizeChanged([railColumn](auto&&, auto const& args) {
            railColumn.Width({args.NewSize().Width < 900 ? 184.0 : 212.0, GridUnitType::Pixel});
        });
        interactionHost = ContentControl();
        interactionHost.FontFamily(Media::FontFamily(L"Segoe UI Variable"));
        interactionHost.RequestedTheme(ElementTheme::Dark);
        interactionHost.ActualThemeChanged([this](auto&&, auto&&) {
            window.AppWindow().TitleBar().PreferredTheme(
                interactionHost.ActualTheme() == ElementTheme::Dark
                    ? winrt::Microsoft::UI::Windowing::TitleBarTheme::Dark
                    : winrt::Microsoft::UI::Windowing::TitleBarTheme::Light);
        });
        interactionHost.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        interactionHost.VerticalContentAlignment(VerticalAlignment::Stretch);
        interactionHost.Content(shell);
        window.Content(interactionHost);
        auto presenter =
            window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>();
        presenter.PreferredMinimumWidth(760);
        presenter.PreferredMinimumHeight(600);
        window.AppWindow().Resize({1240, 940});
        window.AppWindow().Closing([this](auto&&, auto const& args) {
            if (importJob) {
                args.Cancel(true);
                feedback.Text(text(IDS_IMPORT_BUSY));
                return;
            }
            if ((dirty || editorDirty) && !closing) {
                args.Cancel(true);
                confirmClose();
            }
        });
        window.Closed([this](auto&&, auto&&) {
            closed = true;
            timer.Stop();
            RemovePropW(handle, L"CrosshairNative.Settings");
        });
        timer = DispatcherTimer();
        timer.Interval(std::chrono::milliseconds(800));
        timer.Tick([this, step = 0](auto&&, auto&&) mutable -> fire_and_forget {
            if (smokeTest) {
                timer.Stop();
                if (designPreview && step > 0)
                    co_await captureDesign(step);
                if (step < (soakTest ? 120 : designPreview ? 6 : 14)) {
                    const int screen = step % 6;
                    guide = designPreview ? -1 : screen < 4 ? screen : -1;
                    section = designPreview ? std::array<int, 6>{1, 0, 2, 3, 1, 1}[screen]
                              : screen == 4 ? 0
                                            : 2;
                    if (!designPreview && step >= 6 && step < 14) {
                        guide = -1;
                        section = 2;
                        editing = templatePresets()[step - 6];
                        selectedLayer = 0;
                    }
                    if (designPreview) {
                        interactionHost.RequestedTheme(screen == 4 ? ElementTheme::Light
                                                                   : ElementTheme::Dark);
                        window.AppWindow().Resize(screen == 5
                                                      ? winrt::Windows::Graphics::SizeInt32{960, 780}
                                                      : winrt::Windows::Graphics::SizeInt32{1240, 940});
                    }
                    render();
                    if (!designPreview)
                        interactionHost.RequestedTheme(static_cast<ElementTheme>(step % 3));
                    ++step;
                    if (soakTest && (step == 12 || step == 60 || step == 120)) {
                        PROCESS_MEMORY_COUNTERS_EX memory{};
                        memory.cb = sizeof(memory);
                        GetProcessMemoryInfo(GetCurrentProcess(),
                                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                             sizeof(memory));
                        DWORD handles = 0;
                        GetProcessHandleCount(GetCurrentProcess(), &handles);
                        std::printf("WinUI cycles=%d; privateBytes=%zu; handles=%lu; thumbnailCache=%zu\n",
                                    step, memory.PrivateUsage, handles, thumbnails.size());
                        std::fflush(stdout);
                    }
                    timer.Start();
                } else {
                    timer.Stop();
                    choosePreset(L"Slot 1", draft.slots[0].presetId, [](std::string) {
                        std::fputs("Cancelled picker changed assignment\n", stderr);
                        std::exit(1);
                    });
                }
            } else if (!busy)
                poll();
        });
        timer.Start();
        render();
        window.Activate();
        if (smokeTest)
            return;
        if (!FindWindowW(product::engineClass.data(), nullptr)) {
            try {
                launchSibling(L"CrosshairNative.Engine.exe", L"--background");
            } catch (const std::exception& e) {
                error(e.what());
            }
        }
        poll();
    }
    IAsyncAction captureDesign(int step, FrameworkElement target = nullptr) {
        try {
            Media::Imaging::RenderTargetBitmap image;
            co_await image.RenderAsync(target ? target : interactionHost);
            const auto pixels = co_await image.GetPixelsAsync();
            const auto folder = std::filesystem::current_path() / L"out" / L"design-preview";
            std::filesystem::create_directories(folder);
            previewRenderer->writePng(folder / (std::to_wstring(step) + L".png"), image.PixelWidth(),
                                      image.PixelHeight(), {pixels.data(), pixels.Length()});
            std::printf("Design preview %d: %d x %d\n", step, image.PixelWidth(), image.PixelHeight());
        } catch (const hresult_error& e) {
            std::fprintf(stderr, "Design preview failed: %s\n", to_string(e.message()).c_str());
            std::exit(1);
        }
    }
    void error(std::string_view message) {
        feedback.Text(to_hstring(message));
    }
    void changed() {
        dirty = true;
        ++editRevision;
        feedback.Text(text(IDS_DIRTY));
    }
    void nextRequest() {
        if (!busy && !pending.empty()) {
            auto next = std::move(pending.front());
            pending.pop_front();
            next();
        }
    }
    void listProfiles() {
        send(L"ListProfiles", JsonObject(), [this](JsonObject result) {
            profiles.clear();
            profileSelector.Items().Clear();
            for (auto const& item : result.GetNamedArray(L"profiles")) {
                const auto p = item.GetObject();
                profiles.push_back(p);
                profileSelector.Items().Append(box_value(p.GetNamedString(L"name")));
                if (to_string(p.GetNamedString(L"id")) == draft.id)
                    profileSelector.SelectedIndex(static_cast<int>(profiles.size() - 1));
            }
        });
    }
    void newProfile(bool duplicate) {
        if (dirty) {
            feedback.Text(text(IDS_SAVE_FIRST));
            return;
        }
        if (!duplicate)
            draft = initialProfile();
        draft.id = newId();
        draft.name = to_string(text(duplicate ? IDS_COPY_NAME : IDS_NEW_NAME));
        if (!duplicate)
            bindingNames = {};
        guide = 0;
        changed();
        render();
    }
    fire_and_forget send(hstring command, JsonObject payload, std::function<void(JsonObject)> done,
                         std::function<void()> failed = {}) {
        auto lifetime = get_strong();
        if (busy) {
            if (command != L"GetState") {
                if (pending.size() >= 16) {
                    error("Attendi il completamento delle operazioni in corso.");
                    if (failed)
                        failed();
                    co_return;
                }
                pending.push_back([this, command, payload, done = std::move(done),
                                   failed = std::move(failed)] { send(command, payload, done, failed); });
            }
            co_return;
        }
        busy = true;
        const auto id = nextId++;
        JsonObject envelope;
        envelope.Insert(L"version", JsonValue::CreateNumberValue(1));
        envelope.Insert(L"id", JsonValue::CreateNumberValue(id));
        envelope.Insert(L"command", JsonValue::CreateStringValue(command));
        envelope.Insert(L"payload", payload);
        const auto request = to_string(envelope.Stringify());
        apartment_context ui;
        co_await resume_background();
        std::string raw, errorMessage;
        try {
            raw = ipc::call(request);
        } catch (const std::exception& e) {
            errorMessage = e.what();
        }
        co_await ui;
        busy = false;
        if (closed) {
            pending.clear();
            co_return;
        }
        if (!errorMessage.empty()) {
            error(errorMessage);
            if (failed)
                failed();
            nextRequest();
            co_return;
        }
        try {
            checkJsonLimits(raw, ipc::maxPayload);
            auto reply = JsonObject::Parse(to_hstring(raw));
            if (reply.GetNamedNumber(L"version") != 1 || reply.GetNamedNumber(L"id") != id)
                throw std::runtime_error("Risposta del motore non valida.");
            if (!reply.GetNamedBoolean(L"ok")) {
                feedback.Text(reply.GetNamedString(L"error"));
                if (failed)
                    failed();
                nextRequest();
                co_return;
            }
            done(reply.GetNamedObject(L"result"));
        } catch (const std::exception& e) {
            error(e.what());
            if (failed)
                failed();
        } catch (const hresult_error&) {
            error("Risposta del motore non valida. Riprova.");
            if (failed)
                failed();
        }
        nextRequest();
    }
    void poll() {
        send(L"GetState", JsonObject(), [this](JsonObject state) {
            paused = state.GetNamedBoolean(L"paused");
            favorites.clear();
            recents.clear();
            if (state.HasKey(L"favorites"))
                for (auto const& id : state.GetNamedArray(L"favorites"))
                    favorites.push_back(to_string(id.GetString()));
            if (state.HasKey(L"recents"))
                for (auto const& id : state.GetNamedArray(L"recents"))
                    recents.push_back(to_string(id.GetString()));
            autoStart = state.GetNamedBoolean(L"autoStart", false);
            diagnostics = state.GetNamedBoolean(L"diagnostics", false);
            writable = state.GetNamedBoolean(L"writable", true);
            status.Text(text(!state.GetNamedObject(L"profile").GetNamedBoolean(L"enabled")
                                 ? IDS_PROFILE_DISABLED
                             : paused                                            ? IDS_PAUSED
                             : !state.GetNamedBoolean(L"eligible")               ? IDS_WAITING
                             : !state.GetNamedBoolean(L"backendAvailable", true) ? IDS_OVERLAY_UNAVAILABLE
                             : state.GetNamedBoolean(L"hiddenBySlot")            ? IDS_HIDDEN
                             : state.GetNamedNumber(L"selectedSlot") < 0         ? IDS_DEFAULT
                                                                                 : IDS_ACTIVE));
            if (!dirty && !state.GetNamedBoolean(L"testMode")) {
                const bool profileChanged =
                    loaded && configRevision != state.GetNamedNumber(L"configRevision");
                draft = decodeProfile(to_string(state.GetNamedObject(L"profile").Stringify()));
                configRevision = state.GetNamedNumber(L"configRevision");
                if (profileChanged) {
                    bindingNames = {};
                    render();
                    listProfiles();
                }
                if (!loaded) {
                    loaded = true;
                    interactionHost.RequestedTheme(
                        static_cast<ElementTheme>(static_cast<int>(state.GetNamedNumber(L"theme"))));
                    guide = draft.executablePath.empty() ? 0 : -1;
                    render();
                    listProfiles();
                }
            }
            const auto incoming = state.GetNamedNumber(L"captureId");
            if (capturing >= 0 && incoming != captureId && state.HasKey(L"captured")) {
                const auto b = state.GetNamedObject(L"captured");
                Binding binding;
                binding.key = {b.GetNamedString(L"device") == L"keyboard" ? Device::keyboard : Device::mouse,
                               static_cast<std::uint16_t>(b.GetNamedNumber(L"code")),
                               static_cast<std::uint8_t>(b.GetNamedNumber(L"extended"))};
                binding.requiredModifiers = static_cast<std::uint8_t>(b.GetNamedNumber(L"requiredModifiers"));
                binding.allowExtraModifiers = b.GetNamedBoolean(L"allowExtraModifiers");
                binding.action = capturing < 5 ? Action::slot : capturing == 5 ? Action::hide : Action::pause;
                binding.slot = static_cast<std::uint8_t>(capturing < 5 ? capturing : 0);
                const int index = capturing;
                std::erase_if(draft.bindings, [&](const auto& old) { return actionIndex(old) == index; });
                draft.bindings.push_back(binding);
                bindingNames[index] = b.GetNamedString(L"displayName");
                capturing = -1;
                changed();
                render();
            }
            if (capturing >= 0 && !state.GetNamedBoolean(L"capturing")) {
                capturing = -1;
                feedback.Text(text(IDS_CAPTURE_CANCELLED));
            }
            captureId = incoming;
            if (const auto warning = state.GetNamedString(L"warning"); !warning.empty())
                feedback.Text(warning);
        });
    }
    static int actionIndex(const Binding& b) {
        return b.action == Action::slot ? b.slot : b.action == Action::hide ? 5 : 6;
    }
    hstring bindingName(int index) {
        for (const auto& b : draft.bindings)
            if (actionIndex(b) == index) {
                if (!bindingNames[index].empty())
                    return bindingNames[index];
                std::wstring prefix;
                if (b.requiredModifiers & control)
                    prefix += L"Ctrl + ";
                if (b.requiredModifiers & shift)
                    prefix += L"Shift + ";
                if (b.requiredModifiers & alt)
                    prefix += L"Alt + ";
                if (b.key.device == Device::mouse)
                    return hstring(prefix + L"Mouse " + std::to_wstring(b.key.code));
                wchar_t display[128]{};
                GetKeyNameTextW((b.key.code << 16) | (b.key.extended == 1 ? 1 << 24 : 0), display, 128);
                return hstring(prefix + display);
            }
        return text(IDS_UNBOUND);
    }
    Button presets(hstring title, std::string selected, std::function<void(std::string)> assign,
                   bool onBrand = false) {
        Image image;
        image.Width(96);
        image.Height(96);
        Border background;
        background.Background(Media::SolidColorBrush(winrt::Windows::UI::Color{255, 15, 18, 19}));
        background.CornerRadius({8, 8, 8, 8});
        background.Child(image);
        auto caption = label(text(IDS_PRESET_MISSING));
        caption.FontWeight(winrt::Windows::UI::Text::FontWeight{600});
        if (onBrand)
            caption.Style(style(L"BrandTextStyle"));
        try {
            const auto preset = loadPreset(selected);
            image.Source(bitmap(preset, true));
            name(image, to_hstring(preset.name));
            caption.Text(to_hstring(preset.name));
        } catch (const std::exception&) {
        }
        auto details = stack(6);
        details.Children().Append(background);
        details.Children().Append(caption);
        auto choose = muted(text(IDS_CHOOSE_VISUALLY) + L"  ↗", 12);
        if (onBrand)
            choose.Style(style(L"BrandTextStyle"));
        details.Children().Append(choose);
        Button control;
        control.HorizontalAlignment(HorizontalAlignment::Stretch);
        control.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        control.Padding({8, 8, 8, 8});
        control.Content(details);
        name(control, title + L" — " + caption.Text() + L" — " + text(IDS_CHOOSE_VISUALLY));
        auto current = std::make_shared<std::string>(std::move(selected));
        control.Click([this, title, current, assign = std::move(assign), image, caption,
                       weak = make_weak(control)](auto&&, auto&&) {
            choosePreset(
                title, *current, [this, image, caption, assign, current, title, weak](std::string id) {
                    const auto preset = loadPreset(id);
                    image.Source(bitmap(preset, true));
                    name(image, to_hstring(preset.name));
                    caption.Text(to_hstring(preset.name));
                    if (const auto control = weak.get())
                        name(control, title + L" — " + caption.Text() + L" — " + text(IDS_CHOOSE_VISUALLY));
                    *current = id;
                    assign(std::move(id));
                });
        });
        return control;
    }
    fire_and_forget choosePreset(hstring title, std::string selected,
                                 std::function<void(std::string)> assign) {
        const auto lifetime = get_strong();
        if (capturing >= 0) {
            feedback.Text(text(IDS_CAPTURE_HELP));
            co_return;
        }
        const auto profileId = draft.id;
        const auto revision = configRevision;
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Resources().Insert(box_value(L"ContentDialogMaxWidth"), box_value(800.0));
            dialog.Title(box_value(text(IDS_CHOOSE_VISUALLY) + L" — " + title));
            dialog.PrimaryButtonText(text(IDS_ASSIGN_PRESET));
            dialog.DefaultButton(ContentDialogButton::Primary);
            dialog.CloseButtonText(text(IDS_CANCEL_SELECTION));
            dialog.IsPrimaryButtonEnabled(false);
            auto body = stack(10);
            body.Children().Append(muted(text(IDS_PICKER_HELP)));
            body.Children().Append(muted(text(IDS_PREVIEW_SCALE), 12));
            TextBox query;
            query.Header(box_value(text(IDS_SEARCH)));
            name(query, text(IDS_SEARCH));
            body.Children().Append(query);
            auto grid = presetGrid();
            grid.Height(280);
            auto selection = label(text(IDS_PICKER_EMPTY));
            grid.SelectionChanged([weak = make_weak(dialog), selection](auto const& sender, auto&&) {
                const auto grid = sender.template as<GridView>();
                const auto dialog = weak.get();
                if (!dialog)
                    return;
                dialog.IsPrimaryButtonEnabled(false);
                selection.Text(text(IDS_PICKER_EMPTY));
                if (grid.SelectedItem()) {
                    try {
                        const auto preset = loadPreset(to_string(unbox_value<hstring>(grid.SelectedItem())));
                        selection.Text(text(IDS_SELECTED_PRESET) + L" " + to_hstring(preset.name));
                        dialog.IsPrimaryButtonEnabled(true);
                    } catch (const std::exception&) {
                        selection.Text(text(IDS_PRESET_MISSING));
                    }
                }
            });
            const auto filter = [this, grid](std::string words) {
                const auto current = grid.SelectedItem();
                std::transform(words.begin(), words.end(), words.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                auto items = single_threaded_observable_vector<IInspectable>();
                IInspectable retained{nullptr};
                for (const auto& preset : catalog) {
                    auto description = preset.name + " " + preset.family;
                    for (const auto& tag : preset.tags)
                        description += " " + tag;
                    std::transform(description.begin(), description.end(), description.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (description.find(words) == std::string::npos)
                        continue;
                    const auto item = box_value(to_hstring(preset.id));
                    items.Append(item);
                    if (current && unbox_value<hstring>(current) == to_hstring(preset.id))
                        retained = item;
                }
                grid.ItemsSource(items);
                if (retained)
                    grid.SelectedItem(retained);
            };
            query.TextChanged([filter](auto const& sender, auto&&) {
                filter(to_string(sender.template as<TextBox>().Text()));
            });
            filter("");
            for (auto const& item : grid.Items())
                if (unbox_value<hstring>(item) == to_hstring(selected))
                    grid.SelectedItem(item);
            grid.Loaded([](auto const& sender, auto&&) {
                const auto grid = sender.template as<GridView>();
                if (grid.SelectedItem())
                    grid.ScrollIntoView(grid.SelectedItem());
            });
            body.Children().Append(grid);
            body.Children().Append(selection);
            dialog.Content(body);
            DispatcherTimer pickerCheck;
            if (smokeTest) {
                pickerCheck.Interval(std::chrono::milliseconds(250));
                pickerCheck.Tick([this, dialog, query, grid, phase = 0](auto const& sender,
                                                                        auto&&) mutable -> fire_and_forget {
                    const auto check = sender.template as<DispatcherTimer>();
                    check.Stop();
                    const auto require = [phase](bool passed) {
                        if (!passed) {
                            std::fprintf(stderr, "Visual picker check failed at phase %d\n", phase);
                            std::exit(1);
                        }
                    };
                    if (phase == 0) {
                        grid.SelectedIndex(1);
                    } else if (phase == 1) {
                        require(dialog.IsPrimaryButtonEnabled());
                        if (designPreview)
                            co_await captureDesign(7, dialog);
                        query.Text(L"no-such-crosshair-9a59d6");
                    } else if (phase == 2) {
                        require(grid.Items().Size() == 0 && !dialog.IsPrimaryButtonEnabled());
                        query.Text(L"");
                    } else if (phase == 3) {
                        require(grid.Items().Size() >= templatePresets().size());
                        for (auto const& item : grid.Items())
                            require(!to_string(unbox_value<hstring>(item)).starts_with("original-"));
                        grid.SelectedIndex(2);
                    } else {
                        require(dialog.IsPrimaryButtonEnabled());
                        sender.template as<DispatcherTimer>().Stop();
                        dialog.Hide();
                    }
                    ++phase;
                    if (phase <= 4)
                        check.Start();
                });
                pickerCheck.Start();
            }
            const auto result = co_await dialog.ShowAsync();
            pickerCheck.Stop();
            if (result == ContentDialogResult::Primary && grid.SelectedItem()) {
                if (profileId != draft.id || revision != configRevision)
                    throw std::runtime_error("Il profilo è cambiato. Riapri la scelta del mirino.");
                assign(to_string(unbox_value<hstring>(grid.SelectedItem())));
            }
            if (smokeTest) {
                if (dirty || editorDirty) {
                    std::fputs("Navigation changed the profile or preset without user input\n", stderr);
                    std::exit(1);
                }
                std::puts("WinUI smoke passed: slot thumbnails, visual picker, selection, search/reset and "
                          "cancellation; onboarding, catalog, editor and themes constructed.");
                std::fflush(stdout);
                window.Close();
            }
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    void save() {
        if (importJob) {
            feedback.Text(text(IDS_IMPORT_BUSY));
            return;
        }
        try {
            JsonObject payload;
            payload.Insert(L"profile", JsonObject::Parse(to_hstring(encodeProfile(draft))));
            payload.Insert(L"configRevision", JsonValue::CreateNumberValue(configRevision));
            const auto savingRevision = editRevision;
            send(L"SaveProfile", payload, [this, savingRevision](JsonObject result) {
                dirty = editRevision != savingRevision;
                configRevision = result.GetNamedNumber(L"configRevision");
                feedback.Text(text(dirty ? IDS_DIRTY : IDS_SAVED));
                guide = -1;
                render();
                listProfiles();
            });
        } catch (const std::exception& e) {
            error(e.what());
        }
    }
    fire_and_forget confirmClose() {
        auto lifetime = get_strong();
        ContentDialog dialog;
        dialog.XamlRoot(root.XamlRoot());
        dialog.RequestedTheme(interactionHost.RequestedTheme());
        dialog.Title(box_value(text(IDS_CLOSE_TITLE)));
        dialog.PrimaryButtonText(text(IDS_DISCARD));
        dialog.CloseButtonText(text(IDS_CANCEL));
        if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
            closing = true;
            window.Close();
        }
    }
    fire_and_forget reload() {
        auto lifetime = get_strong();
        if (importJob) {
            feedback.Text(text(IDS_IMPORT_BUSY));
            co_return;
        }
        if (dirty) {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_CLOSE_TITLE)));
            dialog.PrimaryButtonText(text(IDS_RELOAD_DISCARD));
            dialog.CloseButtonText(text(IDS_CANCEL));
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
                co_return;
        }
        dirty = false;
        loaded = false;
        bindingNames = {};
        poll();
    }
    void render() {
        page.Children().Clear();
        const int active = guide >= 0 ? (guide == 0 ? 3 : guide == 1 ? 1 : guide == 2 ? 0 : 4) : section;
        const std::array<UINT, 5> titles{IDS_LIBRARY, IDS_BINDINGS, IDS_EDITOR, IDS_SETTINGS, IDS_PREVIEW};
        const std::array<UINT, 5> subtitles{IDS_LIBRARY_SUBTITLE, IDS_SLOTS_SUBTITLE, IDS_EDITOR_SUBTITLE,
                                            IDS_SETTINGS_SUBTITLE, IDS_TEST_SUBTITLE};
        pageTitle.Text(guide >= 0 ? text(IDS_STEP_A + guide) : text(titles[active]));
        pageSubtitle.Text(text(subtitles[active]));
        const std::array<int, 5> indices{1, 0, 2, 3, 4};
        for (unsigned i = 0; i < navigation.size(); ++i)
            navigation[i].IsChecked(guide >= 0 ? i == 4 : indices[i] == active);
        if (active == 3) {
            auto columns = stack();
            auto fields = stack(16);
            fields.Children().Append(label(text(IDS_GAME_PROFILE), 24));
            if (!writable)
                fields.Children().Append(button(text(IDS_RECOVER_PROFILE), [this] { recoverProfile(); }));
            ComboBox model;
            model.Header(box_value(text(IDS_PROFILE)));
            name(model, text(IDS_PROFILE));
            model.Items().Append(box_value(L"Generico"));
            model.Items().Append(box_value(L"Fortnite"));
            model.SelectedIndex(draft.name == "Fortnite" ? 1 : 0);
            model.SelectionChanged([this](auto const& sender, auto&&) {
                const auto choice = sender.template as<ComboBox>().SelectedIndex();
                draft.name = choice == 1 ? "Fortnite" : "Generico";
                changed();
                render();
            });
            fields.Children().Append(model);
            TextBox profileName;
            profileName.Header(box_value(text(IDS_PROFILE_NAME)));
            profileName.Text(to_hstring(draft.name));
            name(profileName, text(IDS_PROFILE_NAME));
            profileName.TextChanged([this](auto const& sender, auto&&) {
                const auto value = to_string(sender.template as<TextBox>().Text());
                if (draft.name != value) {
                    draft.name = value;
                    changed();
                }
            });
            fields.Children().Append(profileName);
            fields.Children().Append(label(text(IDS_CHOOSE_GAME)));
            if (!draft.executablePath.empty())
                fields.Children().Append(
                    label(hstring(L"Bersaglio salvato: " +
                                  std::filesystem::path(draft.executablePath).filename().wstring())));
            ComboBox target;
            target.Header(box_value(text(IDS_TARGET)));
            name(target, text(IDS_TARGET));
            target.HorizontalAlignment(HorizontalAlignment::Stretch);
            target.Items().Append(box_value(text(IDS_NO_TARGET)));
            target.SelectedIndex(0);
            for (std::size_t i = 0; i < targets.size(); ++i) {
                target.Items().Append(box_value(targets[i].GetNamedString(L"title")));
                if (targets[i].GetNamedString(L"executablePath") == draft.executablePath)
                    target.SelectedIndex(static_cast<int>(i + 1));
            }
            target.SelectionChanged([this](auto const& sender, auto&&) {
                const auto i = sender.template as<ComboBox>().SelectedIndex();
                if (i < 1) {
                    draft.executablePath.clear();
                    draft.windowClass.clear();
                } else {
                    draft.executablePath = targets[i - 1].GetNamedString(L"executablePath");
                    draft.windowClass = targets[i - 1].GetNamedString(L"windowClass");
                }
                changed();
            });
            fields.Children().Append(target);
            fields.Children().Append(button(text(IDS_REFRESH), [this] {
                send(L"ListTargets", JsonObject(), [this](JsonObject result) {
                    targets.clear();
                    for (auto const& value : result.GetNamedArray(L"targets"))
                        targets.push_back(value.GetObject());
                    render();
                });
            }));
            ToggleSwitch enabled;
            enabled.Header(box_value(text(IDS_ENABLED)));
            enabled.IsOn(draft.enabled);
            name(enabled, text(IDS_ENABLED));
            enabled.Toggled([this](auto const& sender, auto&&) {
                draft.enabled = sender.template as<ToggleSwitch>().IsOn();
                changed();
            });
            fields.Children().Append(enabled);
            auto offsets = stack(12);
            offsets.Orientation(Orientation::Horizontal);
            for (int axis = 0; axis < 2; ++axis) {
                NumberBox input;
                input.Header(box_value(text(axis ? IDS_CENTER_Y : IDS_CENTER_X)));
                name(input, text(axis ? IDS_CENTER_Y : IDS_CENTER_X));
                input.Minimum(-8192);
                input.Maximum(8192);
                input.Value(axis ? draft.offsetY : draft.offsetX);
                input.ValueChanged([this, axis](auto const& sender, auto&&) {
                    const auto value = sender.template as<NumberBox>().Value();
                    if (std::isfinite(value)) {
                        (axis ? draft.offsetY : draft.offsetX) = static_cast<int>(value);
                        changed();
                    }
                });
                offsets.Children().Append(input);
            }
            fields.Children().Append(wrapControls(offsets, 2, 130));
            fields.Children().Append(button(text(IDS_RESET_CENTER), [this] {
                draft.offsetX = 0;
                draft.offsetY = 0;
                changed();
                render();
            }));
            columns.Children().Append(panel(fields));
            fields = stack(20);
            fields.Children().Append(label(text(IDS_APP_PREFERENCES), 24));
            ComboBox theme;
            theme.Header(box_value(text(IDS_THEME)));
            name(theme, text(IDS_THEME));
            for (UINT i : {IDS_SYSTEM, IDS_LIGHT, IDS_DARK})
                theme.Items().Append(box_value(text(i)));
            theme.SelectedIndex(static_cast<int>(interactionHost.RequestedTheme()));
            theme.SelectionChanged([this](auto const& sender, auto&&) {
                const auto selected = sender.template as<ComboBox>().SelectedIndex();
                JsonObject p;
                p.Insert(L"theme", JsonValue::CreateNumberValue(selected));
                send(L"SetTheme", p, [this, selected](auto&&) {
                    interactionHost.RequestedTheme(static_cast<ElementTheme>(selected));
                });
            });
            fields.Children().Append(theme);
            ToggleSwitch startup;
            startup.Header(box_value(text(IDS_AUTOSTART)));
            name(startup, text(IDS_AUTOSTART));
            startup.IsOn(autoStart);
            startup.Toggled([this](auto const& sender, auto&&) {
                const bool enabled = sender.template as<ToggleSwitch>().IsOn();
                JsonObject p;
                p.Insert(L"enabled", JsonValue::CreateBooleanValue(enabled));
                send(L"SetAutoStart", p, [this, enabled](auto&&) { autoStart = enabled; });
            });
            fields.Children().Append(startup);
            ToggleSwitch logging;
            logging.Header(box_value(text(IDS_DIAGNOSTICS)));
            name(logging, text(IDS_DIAGNOSTICS));
            logging.IsOn(diagnostics);
            logging.Toggled([this](auto const& sender, auto&&) {
                const bool enabled = sender.template as<ToggleSwitch>().IsOn();
                JsonObject p;
                p.Insert(L"enabled", JsonValue::CreateBooleanValue(enabled));
                send(L"SetDiagnostics", p, [this, enabled](auto&&) { diagnostics = enabled; });
            });
            fields.Children().Append(logging);
            fields.Children().Append(label(text(IDS_DIAGNOSTICS_NOTICE)));
            fields.Children().Append(button(text(IDS_DIAGNOSTICS_REPORT), [this] {
                send(L"GetDiagnostics", JsonObject(),
                     [this](JsonObject report) { reviewDiagnostics(report); });
            }));
            columns.Children().Append(panel(fields));
            page.Children().Append(wrapControls(columns, 2, 320));
        } else if (active == 1) {
            auto cards = stack();
            auto shortcuts = stack();
            for (int i = 0; i < 7; ++i) {
                auto row = stack(12);
                if (i < 5) {
                    auto number = muted(hstring(L"0" + std::to_wstring(i + 1)), 24);
                    number.VerticalAlignment(VerticalAlignment::Center);
                    TextBox slot;
                    slot.Text(to_hstring(draft.slots[i].label));
                    name(slot, hstring(L"Slot " + std::to_wstring(i + 1)));
                    slot.TextChanged([this, i](auto const& sender, auto&&) {
                        const auto value = to_string(sender.template as<TextBox>().Text());
                        if (draft.slots[i].label != value) {
                            draft.slots[i].label = value;
                            changed();
                        }
                    });
                    Grid top;
                    ColumnDefinition numberColumn, titleColumn;
                    numberColumn.Width({40, GridUnitType::Pixel});
                    titleColumn.Width({1, GridUnitType::Star});
                    top.ColumnDefinitions().Append(numberColumn);
                    top.ColumnDefinitions().Append(titleColumn);
                    top.Children().Append(number);
                    Grid::SetColumn(slot, 1);
                    top.Children().Append(slot);
                    row.Children().Append(top);
                    row.Children().Append(presets(hstring(L"Slot " + std::to_wstring(i + 1)),
                                                  draft.slots[i].presetId, [this, i](auto id) {
                                                      draft.slots[i].presetId = id;
                                                      changed();
                                                  }));
                } else {
                    row.Children().Append(label(text(i == 5 ? IDS_HIDE : IDS_PAUSE), 20));
                }
                auto capture = button(
                    bindingName(i) == text(IDS_UNBOUND) ? text(IDS_CAPTURE) : bindingName(i), [this, i] {
                        JsonObject p;
                        p.Insert(L"actionIndex", JsonValue::CreateNumberValue(i));
                        send(L"StartCapture", p, [this, i](auto&&) {
                            capturing = i;
                            feedback.Text(text(IDS_CAPTURE_HELP));
                        });
                    });
                name(capture, (i < 5 ? hstring(L"Slot " + std::to_wstring(i + 1))
                                     : text(i == 5 ? IDS_HIDE : IDS_PAUSE)) +
                                  L" — " + bindingName(i) + L" — " + text(IDS_CAPTURE));
                capture.HorizontalAlignment(HorizontalAlignment::Stretch);
                Button options;
                options.Content(box_value(L"···"));
                name(options, text(IDS_KEY_OPTIONS));
                MenuFlyout menu;
                MenuFlyoutItem clear;
                clear.Text(text(IDS_CLEAR));
                clear.Click([this, i](auto&&, auto&&) {
                    std::erase_if(draft.bindings, [i](const auto& b) { return actionIndex(b) == i; });
                    bindingNames[i] = L"";
                    changed();
                    render();
                });
                menu.Items().Append(clear);
                options.Flyout(menu);
                Grid keys;
                ColumnDefinition keyColumn, optionsColumn;
                keyColumn.Width({1, GridUnitType::Star});
                optionsColumn.Width({48, GridUnitType::Pixel});
                keys.ColumnDefinitions().Append(keyColumn);
                keys.ColumnDefinitions().Append(optionsColumn);
                keys.ColumnSpacing(8);
                keys.Children().Append(capture);
                Grid::SetColumn(options, 1);
                keys.Children().Append(options);
                row.Children().Append(keys);
                (i < 5 ? cards : shortcuts).Children().Append(panel(row, 16));
            }
            auto fallback = stack(12);
            auto fallbackTitle = label(text(IDS_FALLBACK), 20);
            fallbackTitle.Style(style(L"BrandTextStyle"));
            fallback.Children().Append(fallbackTitle);
            fallback.Children().Append(presets(
                text(IDS_FALLBACK), draft.defaultPresetId,
                [this](auto id) {
                    draft.defaultPresetId = id;
                    changed();
                },
                true));
            auto fallbackHelp = label(text(IDS_FALLBACK_HELP), 12);
            fallbackHelp.Style(style(L"BrandTextStyle"));
            fallback.Children().Append(fallbackHelp);
            auto fallbackCard = panel(fallback, 16);
            fallbackCard.Style(style(L"BrandPanelStyle"));
            cards.Children().Append(fallbackCard);
            auto slots = wrapControls(cards, 3, 230);
            slots.RowSpacing(16);
            slots.ColumnSpacing(16);
            page.Children().Append(slots);
            page.Children().Append(label(text(IDS_SHORTCUTS), 20));
            page.Children().Append(wrapControls(shortcuts, 2, 260));
        } else if (active == 0) {
            renderLibrary();
        } else if (active == 2) {
            renderEditor();
        } else if (active == 4) {
            page.Children().Append(
                label(L"Il mirino segue il tasto dello slot, non il contenuto dell’inventario."));
            page.Children().Append(button(text(IDS_PREVIEW), [this] {
                try {
                    JsonObject p;
                    p.Insert(L"profile", JsonObject::Parse(to_hstring(encodeProfile(draft))));
                    send(L"StartPreview", p, [this](auto&&) {
                        try {
                            launchSibling(L"CrosshairNative.TestTarget.exe", L"--profile");
                        } catch (const std::exception& e) {
                            error(e.what());
                            send(L"EndTest", JsonObject(), [](auto&&) {});
                        }
                    });
                } catch (const std::exception& e) {
                    error(e.what());
                }
            }));
        }
        if (guide >= 0) {
            auto nav = stack(8);
            nav.Orientation(Orientation::Horizontal);
            if (guide > 0)
                nav.Children().Append(button(text(IDS_BACK), [this] {
                    --guide;
                    render();
                }));
            nav.Children().Append(button(text(guide == 3 ? IDS_FINISH : IDS_NEXT), [this] {
                if (guide == 3)
                    save();
                else {
                    ++guide;
                    render();
                }
            }));
            page.Children().Append(nav);
        }
    }
    void loadCatalog() {
        catalog = templatePresets();
        const auto personal = dataDirectory() / L"presets";
        if (std::filesystem::exists(personal))
            for (const auto& entry : std::filesystem::directory_iterator(personal)) {
                if (catalog.size() >= 2000)
                    break;
                if (!entry.is_regular_file() ||
                    !entry.path().filename().wstring().starts_with(L"preset-user-") ||
                    entry.path().extension() != L".json")
                    continue;
                try {
                    auto p = decodePreset(readJsonFile(entry.path()));
                    if (p.id.starts_with("user-") && entry.path().filename() == "preset-" + p.id + ".json")
                        catalog.push_back(std::move(p));
                } catch (const std::exception&) {
                }
            }
    }
    Media::Imaging::WriteableBitmap bitmap(const Preset& p, bool thumbnail = false) {
        const auto surface = previewRenderer->rasterize(p);
        // Centered thumbnails magnify small shapes without clipping large or off-center presets.
        int extent = thumbnail ? 32 : 128;
        if (thumbnail)
            for (int y = 0; y < 257; ++y)
                for (int x = 0; x < 257; ++x)
                    if (surface.bgra[(y * 257 + x) * 4 + 3])
                        extent = std::max(extent, std::max(std::abs(x - 128), std::abs(y - 128)) + 8);
        extent = std::min(extent, 128);
        const int size = extent * 2 + 1;
        Media::Imaging::WriteableBitmap image(size, size);
        auto pixels = image.PixelBuffer().data();
        for (int y = 0; y < size; ++y)
            std::copy_n(surface.bgra.data() + ((128 - extent + y) * 257 + 128 - extent) * 4, size * 4,
                        pixels + y * size * 4);
        image.Invalidate();
        return image;
    }
    Border preview(const Preset& p, bool thumbnail = false) {
        Image image;
        image.Source(bitmap(p));
        image.Width(thumbnail ? 128 : 257);
        image.Height(thumbnail ? 128 : 257);
        name(image, to_hstring(p.name));
        Border border;
        border.Background(Media::SolidColorBrush(lightPreview ? winrt::Windows::UI::Color{255, 245, 245, 245}
                                                              : winrt::Windows::UI::Color{255, 32, 32, 32}));
        border.Child(image);
        border.CornerRadius({8, 8, 8, 8});
        return border;
    }
    void filterLibrary() {
        auto items = single_threaded_observable_vector<IInspectable>();
        auto lower = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        };
        const auto query = lower(search);
        std::vector<Preset> ordered;
        if (onlyRecent) {
            for (const auto& id : recents)
                for (const auto& p : catalog)
                    if (p.id == id)
                        ordered.push_back(p);
        } else
            ordered = catalog;
        for (const auto& p : ordered) {
            if (onlyFavorites && std::find(favorites.begin(), favorites.end(), p.id) == favorites.end())
                continue;
            std::string words = p.name + ' ' + p.family;
            for (const auto& tag : p.tags)
                words += ' ' + tag;
            if (p.id.starts_with("user-") == personalLibrary &&
                (query.empty() || lower(words).find(query) != std::string::npos))
                items.Append(box_value(to_hstring(p.id)));
        }
        filteringLibrary = true;
        library.ItemsSource(items);
        IInspectable selected{nullptr};
        for (auto const& item : items)
            if (to_string(unbox_value<hstring>(item)) == selectedPreset)
                selected = item;
        if (!selected && items.Size())
            selected = items.GetAt(0);
        library.SelectedItem(selected);
        selectedPreset = selected ? to_string(unbox_value<hstring>(selected)) : "";
        filteringLibrary = false;
        libraryEmpty.Visibility(items.Size() ? Visibility::Collapsed : Visibility::Visible);
        libraryEmpty.Text(text(personalLibrary && search.empty() && !onlyFavorites && !onlyRecent
                                   ? IDS_NO_PERSONAL_PRESETS : IDS_NO_LIBRARY_RESULTS));
        renderLibraryDetails();
    }
    fire_and_forget deletePreset() {
        auto lifetime = get_strong();
        const auto id = selectedPreset;
        if (!id.starts_with("user-"))
            co_return;
        if (draft.defaultPresetId == id || std::any_of(draft.slots.begin(), draft.slots.end(),
                                                      [&](const Slot& slot) { return slot.presetId == id; })) {
            feedback.Text(text(IDS_PRESET_IN_DRAFT));
            co_return;
        }
        if (editing.id == id && editorDirty) {
            feedback.Text(text(IDS_DELETE_EDITOR_DIRTY));
            co_return;
        }
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_DELETE_PRESET)));
            dialog.Content(box_value(to_hstring(loadPreset(id).name) + L"\n\n" + text(IDS_DELETE_PRESET_NOTICE)));
            dialog.PrimaryButtonText(text(IDS_DELETE_PRESET));
            dialog.CloseButtonText(text(IDS_CANCEL));
            dialog.DefaultButton(ContentDialogButton::Close);
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
                co_return;
            JsonObject payload;
            payload.Insert(L"presetId", JsonValue::CreateStringValue(to_hstring(id)));
            send(L"DeletePreset", payload, [this, id](auto&&) {
                std::erase(favorites, id);
                std::erase(recents, id);
                thumbnails.erase(id);
                if (selectedPreset == id)
                    selectedPreset.clear();
                if (editing.id == id) {
                    editing = templatePresets().front();
                    selectedLayer = 0;
                    undo.clear();
                    redo.clear();
                    editorDirty = false;
                }
                loadCatalog();
                render();
                feedback.Text(text(IDS_PRESET_DELETED));
            });
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    void renderLibraryDetails() {
        libraryDetails.Children().Clear();
        if (selectedPreset.empty())
            return;
        try {
            const auto p = loadPreset(selectedPreset);
            libraryDetails.Children().Append(muted(text(IDS_PREVIEW_LABEL), 12));
            libraryDetails.Children().Append(preview(p));
            libraryDetails.Children().Append(label(to_hstring(p.name), 24));
            libraryDetails.Children().Append(muted(text(p.id.starts_with("user-") ? IDS_PERSONAL_PRESETS : IDS_BUILTIN_PRESETS)));
            auto actions = stack(8);
            actions.Orientation(Orientation::Horizontal);
            actions.Children().Append(button(text(IDS_USE_DEFAULT), [this] {
                draft.defaultPresetId = selectedPreset;
                changed();
            }));
            actions.Children().Append(button(text(p.id.starts_with("user-") ? IDS_EDIT_PRESET : IDS_EDIT_COPY),
                                             [this] { openEditor(); }));
            const bool favorite =
                std::find(favorites.begin(), favorites.end(), selectedPreset) != favorites.end();
            actions.Children().Append(
                button(text(favorite ? IDS_UNFAVORITE : IDS_FAVORITE), [this, favorite] {
                    const auto id = selectedPreset;
                    JsonObject p;
                    p.Insert(L"presetId", JsonValue::CreateStringValue(to_hstring(id)));
                    p.Insert(L"favorite", JsonValue::CreateBooleanValue(!favorite));
                    send(L"SetFavorite", p, [this, id, favorite](auto&&) {
                        std::erase(favorites, id);
                        if (!favorite)
                            favorites.push_back(id);
                        renderLibraryDetails();
                        if (onlyFavorites)
                            filterLibrary();
                    });
                }));
            actions.Children().Append(button(text(IDS_PREVIEW_BACKGROUND), [this] {
                lightPreview = !lightPreview;
                renderLibraryDetails();
            }));
            if (p.id.starts_with("user-"))
                actions.Children().Append(button(text(IDS_DELETE_PRESET), [this] { deletePreset(); }));
            libraryDetails.Children().Append(wrapControls(actions, 1));
        } catch (const std::exception& e) {
            libraryDetails.Children().Append(label(to_hstring(e.what())));
        }
    }
    GridView presetGrid() {
        GridView grid;
        grid.Height(460);
        grid.SelectionMode(ListViewSelectionMode::Single);
        name(grid, text(IDS_LIBRARY));
        grid.ItemTemplate(
            Markup::XamlReader::Load(
                LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><StackPanel Width="144" Margin="6"><Border Background="#0F1213" CornerRadius="8"><Image x:Name="Thumbnail" Width="112" Height="112" /></Border><TextBlock x:Name="Caption" TextWrapping="Wrap" FontSize="13" FontWeight="SemiBold" Margin="4,10,4,4" MinHeight="36" /></StackPanel></DataTemplate>)")
                .as<DataTemplate>());
        grid.ContainerContentChanging([this](auto&&, ContainerContentChangingEventArgs const& args) {
            const auto content = args.ItemContainer().ContentTemplateRoot().try_as<FrameworkElement>();
            if (!content)
                return;
            const auto image = content.FindName(L"Thumbnail").as<Image>();
            const auto caption = content.FindName(L"Caption").as<TextBlock>();
            image.Source(nullptr);
            if (args.InRecycleQueue())
                return;
            const auto id = to_string(unbox_value<hstring>(args.Item()));
            try {
                const auto p = loadPreset(id);
                caption.Text(to_hstring(p.name));
                name(args.ItemContainer(), to_hstring(p.name));
                if (!thumbnails.contains(id)) {
                    if (thumbnails.size() >= 48)
                        thumbnails.clear();
                    thumbnails.emplace(id, bitmap(p, true));
                }
                image.Source(thumbnails.at(id));
            } catch (const std::exception&) {
                caption.Text(text(IDS_PRESET_MISSING));
            }
            args.Handled(true);
        });
        return grid;
    }
    void renderLibrary() {
        auto results = stack(16);
        results.Children().Append(
            muted(to_hstring(std::to_string(catalog.size())) + L" " + text(IDS_PRESET_COUNT)));
        auto transfer = stack(8);
        transfer.Orientation(Orientation::Horizontal);
        transfer.Children().Append(button(text(IDS_IMPORT_FILES), [this] { importFiles(); }));
        transfer.Children().Append(button(text(IDS_EXPORT_PROFILE), [this] { exportProfile(); }));
        transfer.Children().Append(button(text(IDS_EXPORT_PRESET), [this] { exportPreset(); }));
        Expander files;
        files.Header(box_value(text(IDS_IMPORT_EXPORT)));
        files.Content(wrapControls(transfer, 3, 160));
        TextBox query;
        query.Header(box_value(text(IDS_SEARCH)));
        name(query, text(IDS_SEARCH));
        query.Text(to_hstring(search));
        query.TextChanged([this](auto const& sender, auto&&) {
            search = to_string(sender.template as<TextBox>().Text());
            filterLibrary();
        });
        auto searchFields = stack();
        searchFields.Children().Append(query);
        results.Children().Append(searchFields);
        auto categories = stack(12);
        categories.Orientation(Orientation::Horizontal);
        for (bool personal : {false, true}) {
            RadioButton category;
            const auto count = std::count_if(catalog.begin(), catalog.end(), [personal](const Preset& p) {
                return p.id.starts_with("user-") == personal;
            });
            const auto caption = text(personal ? IDS_PERSONAL_PRESETS : IDS_BUILTIN_PRESETS);
            category.Content(box_value(caption + L" (" + to_hstring(count) + L")"));
            name(category, caption);
            category.GroupName(L"LibraryCategory");
            category.IsChecked(personalLibrary == personal);
            category.Checked([this, personal](auto&&, auto&&) {
                if (personalLibrary != personal) {
                    personalLibrary = personal;
                    filterLibrary();
                }
            });
            categories.Children().Append(category);
        }
        const auto categoryControls = wrapControls(categories, 2, 160);
        results.Children().Append(categoryControls);
        auto filters = stack(8);
        filters.Orientation(Orientation::Horizontal);
        for (const auto [id, recent] :
             std::array<std::pair<UINT, bool>, 2>{{{IDS_ONLY_FAVORITES, false}, {IDS_ONLY_RECENT, true}}}) {
            CheckBox filter;
            filter.Content(box_value(text(id)));
            name(filter, text(id));
            filter.IsChecked(recent ? onlyRecent : onlyFavorites);
            filter.Click([this, recent](auto const& sender, auto&&) {
                const bool checked = sender.template as<CheckBox>().IsChecked().Value();
                (recent ? onlyRecent : onlyFavorites) = checked;
                filterLibrary();
            });
            filters.Children().Append(filter);
        }
        results.Children().Append(wrapControls(filters, 2, 150));
        library = presetGrid();
        library.SelectionChanged([this](auto&&, auto&&) {
            if (!filteringLibrary && library.SelectedItem()) {
                selectedPreset = to_string(unbox_value<hstring>(library.SelectedItem()));
                renderLibraryDetails();
                JsonObject p;
                p.Insert(L"presetId", JsonValue::CreateStringValue(to_hstring(selectedPreset)));
                send(L"MarkRecent", p, [](auto&&) {});
            }
        });
        libraryEmpty = muted(L"");
        results.Children().Append(libraryEmpty);
        results.Children().Append(library);
        libraryDetails = stack(16);
        auto inspector = panel(libraryDetails);
        inspector.VerticalAlignment(VerticalAlignment::Top);
        page.Children().Append(withInspector(results, inspector));
        page.Children().Append(files);
        filterLibrary();
        if (smokeTest) {
            const bool previousCategory = personalLibrary;
            for (bool personal : {true, false}) {
                categoryControls.Children().GetAt(personal ? 1 : 0).as<RadioButton>().IsChecked(true);
                for (auto const& item : library.Items())
                    if (to_string(unbox_value<hstring>(item)).starts_with("user-") != personal)
                        throw std::runtime_error("Library category contains an incorrect preset");
                if (!library.Items().Size() && (!selectedPreset.empty() || libraryDetails.Children().Size()))
                    throw std::runtime_error("Empty library retains a stale selection");
            }
            categoryControls.Children().GetAt(previousCategory ? 1 : 0).as<RadioButton>().IsChecked(true);
            std::puts("Library categories passed: built-in/personal separation and empty selection.");
        }
    }
    fire_and_forget openEditor() {
        auto lifetime = get_strong();
        if (editorDirty) {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_CLOSE_TITLE)));
            dialog.PrimaryButtonText(text(IDS_DISCARD_EDITOR));
            dialog.CloseButtonText(text(IDS_CANCEL));
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
                co_return;
        }
        try {
            editing = loadPreset(selectedPreset);
            undo.clear();
            redo.clear();
            editorDirty = false;
            selectedLayer = 0;
            advancedLayers = false;
            guide = -1;
            section = 2;
            render();
        } catch (const std::exception& e) {
            error(e.what());
        }
    }
    bool edit(const std::function<void(Preset&)>& change, bool rebuild = false) {
        auto candidate = editing;
        change(candidate);
        if (auto invalid = validate(candidate)) {
            error(*invalid);
            return false;
        }
        if (undo.size() >= 64)
            undo.erase(undo.begin());
        undo.push_back(editing);
        redo.clear();
        editing = std::move(candidate);
        editorDirty = true;
        if (rebuild)
            render();
        else if (editorPreview)
            editorPreview.Source(bitmap(editing));
        feedback.Text(text(IDS_EDITOR_DIRTY));
        return true;
    }
    void importPng() {
        try {
            if (editing.layers.size() >= 16)
                throw std::runtime_error("Rimuovi un livello prima di importare un PNG.");
            const auto chosen = chooseFile(handle, false, text(IDS_IMPORT_PNG).c_str(), L"png", L"*.png");
            if (!chosen)
                return;
            const auto decoded = previewRenderer->readPng(*chosen);
            const auto id = "user-" + newId();
            const auto stage = dataDirectory() / L"staging" / ("asset-" + id + ".png");
            rejectReparsePath(stage);
            std::filesystem::create_directories(stage.parent_path());
            std::filesystem::copy_file(*chosen, stage);
            JsonObject payload;
            payload.Insert(L"assetId", JsonValue::CreateStringValue(to_hstring(id)));
            const float scale = std::min(1.f, 192.f / std::max(decoded.width, decoded.height));
            const auto width = decoded.width * scale, height = decoded.height * scale;
            send(L"ImportAsset", payload, [this, id, width, height](auto&&) {
                edit(
                    [&](Preset& p) {
                        Layer image;
                        image.shape = Shape::png;
                        image.assetId = id;
                        image.width = width;
                        image.height = height;
                        image.outline = 0;
                        p.layers.push_back(image);
                        p.provenance = "composizione personale con PNG importato";
                        p.author.clear();
                        p.source.clear();
                        p.license.clear();
                        p.permission.clear();
                    },
                    true);
                feedback.Text(text(IDS_PNG_IMPORTED));
            });
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    void exportPreset() {
        try {
            const auto path = chooseFile(handle, true, text(IDS_EXPORT_PRESET).c_str(), L"json", L"*.json");
            if (!path)
                return;
            const auto p = loadPreset(selectedPreset);
            if (std::any_of(p.layers.begin(), p.layers.end(),
                            [](const auto& l) { return l.shape == Shape::png; }))
                throw std::runtime_error("Per includere il PNG assegna il mirino a uno slot ed esporta il "
                                         "profilo come pacchetto.");
            writeJsonFile(*path, encodePreset(p));
            feedback.Text(text(IDS_EXPORTED));
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    void exportProfile() {
        try {
            Pack pack;
            pack.profile = draft;
            std::vector<std::string> ids{draft.defaultPresetId};
            for (const auto& slot : draft.slots)
                if (std::find(ids.begin(), ids.end(), slot.presetId) == ids.end())
                    ids.push_back(slot.presetId);
            for (const auto& id : ids) {
                auto p = loadPreset(id);
                for (const auto& l : p.layers)
                    if (l.shape == Shape::png &&
                        std::none_of(pack.assets.begin(), pack.assets.end(),
                                     [&](const auto& a) { return a.id == l.assetId; })) {
                        const auto path = dataDirectory() / L"assets" / ("asset-" + l.assetId + ".png");
                        previewRenderer->readPng(path);
                        const auto bytes = readJsonFile(path, 16 * 1024 * 1024);
                        pack.assets.push_back({l.assetId, {bytes.begin(), bytes.end()}});
                    }
                pack.presets.push_back(std::move(p));
            }
            const auto content = encodePack(pack);
            const auto file = chooseFile(handle, true, text(IDS_EXPORT_PROFILE).c_str(), L"crosshairpack",
                                         L"*.crosshairpack");
            if (!file)
                return;
            writeJsonFile(*file, content, maxPackBytes);
            feedback.Text(text(IDS_EXPORTED));
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    fire_and_forget recoverProfile() {
        const auto lifetime = get_strong();
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_RECOVER_PROFILE)));
            dialog.Content(box_value(text(IDS_RECOVER_NOTICE)));
            dialog.PrimaryButtonText(text(IDS_RECOVER_PROFILE));
            dialog.CloseButtonText(text(IDS_CANCEL));
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
                JsonObject p;
                p.Insert(L"confirmed", JsonValue::CreateBooleanValue(true));
                p.Insert(L"configRevision", JsonValue::CreateNumberValue(configRevision));
                send(L"RecoverProfile", p, [this](auto&&) {
                    dirty = false;
                    loaded = false;
                    poll();
                });
            }
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    fire_and_forget reviewDiagnostics(JsonObject report) {
        const auto lifetime = get_strong();
        try {
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_DIAGNOSTICS_REPORT)));
            TextBox preview;
            preview.Text(report.Stringify());
            preview.IsReadOnly(true);
            preview.TextWrapping(TextWrapping::Wrap);
            preview.AcceptsReturn(true);
            dialog.Content(preview);
            dialog.PrimaryButtonText(text(IDS_SAVE_REPORT));
            dialog.CloseButtonText(text(IDS_CANCEL));
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary) {
                const auto file = chooseFile(handle, true, text(IDS_SAVE_REPORT).c_str(), L"json", L"*.json");
                if (file)
                    writeJsonFile(*file, to_string(report.Stringify()));
            }
        } catch (const std::exception& e) {
            error(e.what());
        } catch (const hresult_error& e) {
            feedback.Text(e.message());
        }
    }
    void cleanupImport() {
        if (importJob)
            for (const auto& asset : importJob->assets) {
                const auto stage = dataDirectory() / L"staging" / ("asset-" + asset.id + ".png");
                DeleteFileW(stage.c_str());
            }
        importJob.reset();
        interactionHost.IsEnabled(true);
    }
    void importNext() {
        if (!importJob)
            return;
        const auto fail = [this] {
            cleanupImport();
            loadCatalog();
            feedback.Text(feedback.Text() + L" " + text(IDS_IMPORT_PARTIAL));
        };
        if (importAssetIndex < importJob->assets.size()) {
            JsonObject p;
            p.Insert(L"assetId",
                     JsonValue::CreateStringValue(to_hstring(importJob->assets[importAssetIndex].id)));
            send(
                L"ImportAsset", p,
                [this](auto&&) {
                    ++importAssetIndex;
                    importNext();
                },
                fail);
            return;
        }
        if (importPresetIndex < importJob->presets.size()) {
            JsonObject p;
            p.Insert(L"preset",
                     JsonObject::Parse(to_hstring(encodePreset(importJob->presets[importPresetIndex]))));
            send(
                L"SavePreset", p,
                [this](auto&&) {
                    ++importPresetIndex;
                    feedback.Text(text(IDS_IMPORT_PROGRESS) + L" " +
                                  to_hstring(std::to_string(importPresetIndex)));
                    importNext();
                },
                fail);
            return;
        }
        draft = importJob->profile;
        bindingNames = {};
        cleanupImport();
        loadCatalog();
        changed();
        guide = 0;
        render();
        feedback.Text(text(IDS_IMPORTED));
    }
    fire_and_forget importFiles() {
        auto lifetime = get_strong();
        if (importJob) {
            feedback.Text(text(IDS_IMPORT_BUSY));
            co_return;
        }
        if (dirty) {
            feedback.Text(text(IDS_SAVE_FIRST));
            co_return;
        }
        try {
            const auto file = chooseFile(handle, false, text(IDS_IMPORT_FILES).c_str(), L"crosshairpack",
                                         L"*.crosshairpack;*.json");
            if (!file)
                co_return;
            rejectReparsePath(*file);
            auto extension = file->extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            if (extension == L".json") {
                auto p = decodePreset(readJsonFile(*file));
                previewRenderer->rasterize(p);
                p.id = "user-" + newId();
                p.family = "personali";
                ContentDialog dialog;
                dialog.XamlRoot(root.XamlRoot());
                dialog.RequestedTheme(interactionHost.RequestedTheme());
                dialog.Title(box_value(text(IDS_IMPORT_CONFIRM)));
                dialog.Content(box_value(to_hstring(p.name) + L"\n" + text(IDS_IMPORT_NOTICE)));
                dialog.PrimaryButtonText(text(IDS_IMPORT_ACTION));
                dialog.CloseButtonText(text(IDS_CANCEL_IMPORT));
                if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
                    co_return;
                JsonObject payload;
                payload.Insert(L"preset", JsonObject::Parse(to_hstring(encodePreset(p))));
                send(L"SavePreset", payload, [this, p](auto&&) {
                    selectedPreset = p.id;
                    loadCatalog();
                    render();
                    feedback.Text(text(IDS_PRESET_SAVED));
                });
                co_return;
            }
            if (extension != L".crosshairpack")
                throw std::runtime_error("Formato non supportato.");
            auto pack = decodePack(readJsonFile(*file, maxPackBytes));
            std::map<std::string, std::string> presetIds, assetIds;
            for (auto& asset : pack.assets) {
                const auto id = "user-" + newId();
                assetIds.emplace(asset.id, id);
                asset.id = id;
            }
            for (auto& preset : pack.presets) {
                const auto id = "user-" + newId();
                presetIds.emplace(preset.id, id);
                preset.id = id;
                preset.family = "personali";
                for (auto& l : preset.layers)
                    if (l.shape == Shape::png)
                        l.assetId = assetIds.at(l.assetId);
            }
            pack.profile.id = newId();
            pack.profile.executablePath.clear();
            pack.profile.windowClass.clear();
            pack.profile.offsetX = 0;
            pack.profile.offsetY = 0;
            pack.profile.defaultPresetId = presetIds.at(pack.profile.defaultPresetId);
            for (auto& slot : pack.profile.slots)
                slot.presetId = presetIds.at(slot.presetId);
            validatePack(pack);
            importJob = std::make_shared<Pack>(std::move(pack));
            for (const auto& asset : importJob->assets) {
                const auto stage = dataDirectory() / L"staging" / ("asset-" + asset.id + ".png");
                rejectReparsePath(stage);
                std::filesystem::create_directories(stage.parent_path());
                const auto assetFile = CreateFileW(stage.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                                   FILE_ATTRIBUTE_NORMAL, nullptr);
                if (assetFile == INVALID_HANDLE_VALUE)
                    throw std::runtime_error("Staging importazione non disponibile.");
                DWORD written = 0;
                const bool ok = WriteFile(assetFile, asset.png.data(), static_cast<DWORD>(asset.png.size()),
                                          &written, nullptr) &&
                                written == asset.png.size();
                CloseHandle(assetFile);
                if (!ok)
                    throw std::runtime_error("Staging importazione incompleto.");
                previewRenderer->readPng(stage);
            }
            ContentDialog dialog;
            dialog.XamlRoot(root.XamlRoot());
            dialog.RequestedTheme(interactionHost.RequestedTheme());
            dialog.Title(box_value(text(IDS_IMPORT_CONFIRM)));
            dialog.Content(box_value(to_hstring(importJob->profile.name) + L"\n" +
                                     to_hstring(std::to_string(importJob->presets.size())) + L" preset, " +
                                     to_hstring(std::to_string(importJob->assets.size())) + L" PNG\n" +
                                     text(IDS_IMPORT_NOTICE)));
            dialog.PrimaryButtonText(text(IDS_IMPORT_ACTION));
            dialog.CloseButtonText(text(IDS_CANCEL_IMPORT));
            if (co_await dialog.ShowAsync() != ContentDialogResult::Primary) {
                cleanupImport();
                co_return;
            }
            interactionHost.IsEnabled(false);
            importAssetIndex = 0;
            importPresetIndex = 0;
            importNext();
        } catch (const std::exception& e) {
            cleanupImport();
            error(e.what());
        } catch (const hresult_error& e) {
            cleanupImport();
            feedback.Text(e.message());
        }
    }
    void saveEditedPreset(bool asNew) {
        const auto snapshot = encodePreset(editing);
        auto saved = editing;
        if (asNew)
            saved.id = "user-" + newId();
        saved.family = "personali";
        JsonObject p;
        p.Insert(L"preset", JsonObject::Parse(to_hstring(encodePreset(saved))));
        send(asNew ? L"SavePreset" : L"UpdatePreset", p, [this, saved, snapshot](auto&&) {
            if (encodePreset(editing) == snapshot) {
                if (editing.id != saved.id) {
                    undo.clear();
                    redo.clear();
                }
                editing = saved;
                selectedPreset = saved.id;
                personalLibrary = true;
                editorDirty = false;
            }
            thumbnails.erase(saved.id);
            loadCatalog();
            render();
            feedback.Text(text(IDS_PRESET_SAVED));
        });
    }
    void renderEditor() {
        auto fields = stack(16);
        auto inspector = stack(16);
        inspector.Children().Append(muted(text(IDS_PREVIEW_LABEL), 12));
        TextBox title;
        title.Header(box_value(text(IDS_PRESET_NAME)));
        title.Text(to_hstring(editing.name));
        name(title, text(IDS_PRESET_NAME));
        title.TextChanged([this](auto const& sender, auto&&) {
            const auto value = to_string(sender.template as<TextBox>().Text());
            if (value != editing.name && !edit([&](Preset& p) { p.name = value; }))
                sender.template as<TextBox>().Text(to_hstring(editing.name));
        });
        fields.Children().Append(title);
        Expander provenance;
        provenance.Header(box_value(text(IDS_PROVENANCE)));
        auto provenanceFields = stack(8);
        provenanceFields.Children().Append(label(text(IDS_PROVENANCE_NOTICE)));
        for (const auto [id, member] :
             std::array<std::pair<UINT, std::string Preset::*>, 5>{{{IDS_ORIGIN, &Preset::provenance},
                                                                    {IDS_AUTHOR, &Preset::author},
                                                                    {IDS_SOURCE, &Preset::source},
                                                                    {IDS_LICENSE, &Preset::license},
                                                                    {IDS_PERMISSION, &Preset::permission}}}) {
            TextBox field;
            field.Header(box_value(text(id)));
            name(field, text(id));
            field.MaxLength(256);
            field.Text(to_hstring(editing.*member));
            field.TextChanged([this, member](auto const& sender, auto&&) {
                const auto value = to_string(sender.template as<TextBox>().Text());
                if (value != editing.*member)
                    edit([&](Preset& p) { p.*member = value; });
            });
            provenanceFields.Children().Append(field);
        }
        provenance.Content(provenanceFields);

        const auto surface = preview(editing);
        editorPreview = surface.Child().as<Image>();
        inspector.Children().Append(surface);
        const bool personal = editing.id.starts_with("user-");
        auto savePreset = button(text(personal ? IDS_SAVE_PRESET_CHANGES : IDS_SAVE_NEW_PRESET),
                                 [this, personal] { saveEditedPreset(!personal); });
        savePreset.Style(style(L"PrimaryButtonStyle"));
        savePreset.HorizontalAlignment(HorizontalAlignment::Stretch);
        inspector.Children().Append(savePreset);
        if (personal)
            inspector.Children().Append(button(text(IDS_SAVE_NEW_PRESET), [this] { saveEditedPreset(true); }));
        auto actions = stack(8);
        actions.Orientation(Orientation::Horizontal);
        actions.Children().Append(button(text(IDS_UNDO), [this] {
            if (!undo.empty()) {
                redo.push_back(editing);
                editing = undo.back();
                undo.pop_back();
                selectedLayer = std::min(selectedLayer, static_cast<int>(editing.layers.size() - 1));
                editorDirty = true;
                render();
            }
        }));
        actions.Children().Append(button(text(IDS_REDO), [this] {
            if (!redo.empty()) {
                undo.push_back(editing);
                editing = redo.back();
                redo.pop_back();
                selectedLayer = std::min(selectedLayer, static_cast<int>(editing.layers.size() - 1));
                editorDirty = true;
                render();
            }
        }));
        actions.Children().Append(button(text(IDS_RESET_PRESET), [this] {
            edit([&](Preset& p) { p = loadPreset(editing.id); }, true);
        }));
        actions.Children().Append(button(text(IDS_PREVIEW_BACKGROUND), [this] {
            lightPreview = !lightPreview;
            render();
        }));
        inspector.Children().Append(wrapControls(actions, 2, 110));
        inspector.Children().Append(muted(text(IDS_EDITOR_HELP), 12));
        auto quick = stack(8);
        quick.Children().Append(label(text(IDS_CUSTOMIZE), 20));
        quick.Children().Append(muted(text(IDS_CUSTOMIZE_HELP)));
        const auto numericPreset = [this](UINT title, float value, float minimum, float maximum,
                                         std::function<void(Preset&, float)> assign,
                                         double step = .5, hstring unit = L"px") {
            auto control = stack(4);
            const auto caption = text(title) + L" (" + unit + L")";
            control.Children().Append(label(caption));
            Slider slider;
            name(slider, caption);
            slider.Minimum(minimum);
            slider.Maximum(maximum);
            slider.StepFrequency(step);
            slider.SmallChange(step);
            slider.LargeChange(step * 10);
            slider.Value(value);
            slider.VerticalAlignment(VerticalAlignment::Center);
            NumberBox field;
            name(field, caption);
            field.Minimum(minimum);
            field.Maximum(maximum);
            field.SmallChange(step);
            field.Value(value);
            field.Width(80);
            // Weak controls avoid event-handler cycles; one accepted value keeps both inputs in sync.
            auto accepted = std::make_shared<double>(value);
            const auto update = [this, assign, accepted, weakSlider = make_weak(slider),
                                 weakField = make_weak(field)](double value) {
                const auto slider = weakSlider.get();
                const auto field = weakField.get();
                if (!slider || !field || value == *accepted)
                    return;
                if (std::isfinite(value) && edit([&](Preset& p) { assign(p, static_cast<float>(value)); }))
                    *accepted = value;
                slider.Value(*accepted);
                field.Value(*accepted);
            };
            slider.ValueChanged([update](auto&&, auto const& args) { update(args.NewValue()); });
            field.ValueChanged([update](auto const& sender, auto&&) {
                update(sender.template as<NumberBox>().Value());
            });
            Grid row;
            row.ColumnSpacing(12);
            ColumnDefinition track, number;
            track.Width({1, GridUnitType::Star});
            number.Width({80, GridUnitType::Pixel});
            row.ColumnDefinitions().Append(track);
            row.ColumnDefinitions().Append(number);
            Grid::SetColumn(field, 1);
            row.Children().Append(slider);
            row.Children().Append(field);
            control.Children().Append(row);
            return control;
        };
        auto dimensions = stack(8);
        const auto addArmControls = [&](auto geometry, auto read, auto write) {
            for (const bool vertical : {false, true}) {
                if ((vertical ? geometry.verticalLength : geometry.horizontalLength) == 0)
                    continue;
                for (const bool gap : {false, true}) {
                    const UINT title = gap ? (vertical ? IDS_VERTICAL_GAP : IDS_HORIZONTAL_GAP)
                                           : (vertical ? IDS_VERTICAL_ARM_LENGTH : IDS_HORIZONTAL_ARM_LENGTH);
                    const float value = gap ? (vertical ? geometry.verticalGap : geometry.horizontalGap)
                                            : (vertical ? geometry.verticalLength : geometry.horizontalLength);
                    dimensions.Children().Append(numericPreset(title, value, gap ? 0 : .5f, 80,
                        [read, write, vertical, gap](Preset& p, float v) {
                            if (auto current = read(p)) {
                                auto& value = gap ? (vertical ? current->verticalGap : current->horizontalGap)
                                                  : (vertical ? current->verticalLength : current->horizontalLength);
                                value = v;
                                write(p, *current);
                            }
                        }));
                }
            }
        };
        const auto arms = armGeometry(editing);
        const auto corners = cornerGeometry(editing);
        const auto chevron = chevronGeometry(editing);
        const bool dots = std::all_of(editing.layers.begin(), editing.layers.end(), [](const Layer& l) {
            return l.shape == Shape::dot;
        });
        const bool centered = std::all_of(editing.layers.begin(), editing.layers.end(), [](const Layer& l) {
            return (l.shape == Shape::dot || l.shape == Shape::ring) && l.x == 0 && l.y == 0;
        });
        if (arms)
            addArmControls(*arms, armGeometry, setArmGeometry);
        else if (corners)
            addArmControls(*corners, cornerGeometry, setCornerGeometry);
        else if (chevron) {
            dimensions.Children().Append(numericPreset(IDS_LINE_LENGTH, chevron->length, .5f, 80,
                [](Preset& p, float v) {
                    if (auto g = chevronGeometry(p)) { g->length = v; setChevronGeometry(p, *g); }
                }));
            dimensions.Children().Append(numericPreset(IDS_CHEVRON_ANGLE, chevron->angle, 1, 89,
                [](Preset& p, float v) {
                    if (auto g = chevronGeometry(p)) { g->angle = v; setChevronGeometry(p, *g); }
                }, 1, L"°"));
            dimensions.Children().Append(numericPreset(IDS_CROSS_GAP, chevron->gap, 0, 80,
                [](Preset& p, float v) {
                    if (auto g = chevronGeometry(p)) { g->gap = v; setChevronGeometry(p, *g); }
                }));
        } else if (dots && !centered) {
            dimensions.Children().Append(numericPreset(IDS_DOT_SPACING, dotSpacing(editing), .5f, 80,
                [](Preset& p, float v) { setDotSpacing(p, v); }));
            const auto outer = std::find_if(editing.layers.begin(), editing.layers.end(), [](const Layer& l) {
                return l.x != 0 || l.y != 0;
            });
            dimensions.Children().Append(numericPreset(IDS_OUTER_DOT_DIAMETER, outer->radius * 2, 1, 32,
                [](Preset& p, float v) {
                    for (auto& l : p.layers)
                        if (l.x != 0 || l.y != 0) l.radius = v / 2;
                }));
        }
        const bool custom = !arms && !corners && !chevron && !dots && !centered;
        for (std::size_t i = 0; i < editing.layers.size(); ++i) {
            const auto& l = editing.layers[i];
            if (custom) {
                dimensions.Children().Append(label(text(IDS_LAYER) + L" " + to_hstring(i + 1), 16));
                if (l.shape == Shape::line)
                    dimensions.Children().Append(numericPreset(IDS_LINE_LENGTH,
                        std::hypot(l.endX - l.x, l.endY - l.y), .5f, 160,
                        [i](Preset& p, float v) { setLineLength(p.layers[i], v); }));
                for (const bool vertical : {false, true}) {
                    const float position = l.shape == Shape::line
                        ? (vertical ? (l.y + l.endY) / 2 : (l.x + l.endX) / 2)
                        : (vertical ? l.y : l.x);
                    dimensions.Children().Append(numericPreset(vertical ? IDS_POSITION_Y : IDS_POSITION_X,
                        position, -96, 96, [i, vertical](Preset& p, float v) {
                            auto& l = p.layers[i];
                            auto& start = vertical ? l.y : l.x;
                            auto& end = vertical ? l.endY : l.endX;
                            if (l.shape == Shape::line) {
                                const float delta = v - (start + end) / 2;
                                start += delta;
                                end += delta;
                            } else start = v;
                        }));
                }
                if (l.shape == Shape::png) {
                    dimensions.Children().Append(numericPreset(IDS_WIDTH, l.width, .5f, 192,
                        [i](Preset& p, float v) { p.layers[i].width = v; }));
                    dimensions.Children().Append(numericPreset(IDS_HEIGHT, l.height, .5f, 192,
                        [i](Preset& p, float v) { p.layers[i].height = v; }));
                }
            }
            if (l.shape == Shape::ring ||
                (l.shape == Shape::dot && (editing.layers.size() == 1 || (custom && (l.x != 0 || l.y != 0)))))
                dimensions.Children().Append(numericPreset(IDS_DIAMETER, l.radius * 2, 1, 160,
                    [i](Preset& p, float v) { p.layers[i].radius = v / 2; }));
        }
        if (std::any_of(editing.layers.begin(), editing.layers.end(), [](const Layer& l) {
                return l.shape == Shape::line || l.shape == Shape::ring;
            }))
            dimensions.Children().Append(numericPreset(IDS_THICKNESS, editing.layers.front().thickness, .5f, 16,
            [](Preset& p, float v) { for (auto& l : p.layers) l.thickness = v; }));
        dimensions.Children().Append(numericPreset(IDS_OUTLINE, editing.layers.front().outline, 0, 8,
            [](Preset& p, float v) { for (auto& l : p.layers) l.outline = v; }));
        const auto dimensionControls = wrapControls(dimensions, 2);
        quick.Children().Append(dimensionControls);
        const auto centerDot = [](const Layer& l) { return l.shape == Shape::dot && l.x == 0 && l.y == 0; };
        const auto dot = std::find_if(editing.layers.begin(), editing.layers.end(), centerDot);
        if (editing.layers.size() > 1 || dot == editing.layers.end()) {
            ToggleSwitch center;
            center.Header(box_value(text(IDS_CENTER_DOT)));
            name(center, text(IDS_CENTER_DOT));
            center.IsOn(dot != editing.layers.end());
            center.Toggled([this, centerDot](auto const& sender, auto&&) {
                const bool enabled = sender.template as<ToggleSwitch>().IsOn();
                selectedLayer = 0;
                if (!edit([&](Preset& p) {
                    if (enabled) {
                        Layer dot;
                        dot.radius = 1;
                        dot.color = p.layers.front().color;
                        dot.outline = p.layers.front().outline;
                        p.layers.push_back(dot);
                    } else
                        std::erase_if(p.layers, centerDot);
                }, true))
                    render();
            });
            quick.Children().Append(center);
        }
        if (dot != editing.layers.end() && editing.layers.size() > 1)
            quick.Children().Append(numericPreset(IDS_DOT_RADIUS, dot->radius, .5f, 16,
                [centerDot](Preset& p, float v) { for (auto& l : p.layers) if (centerDot(l)) l.radius = v; }));
        quick.Children().Append(label(text(IDS_COLOR), 16));
        const auto currentColor = editing.layers.front().color;
        const winrt::Windows::UI::Color initialColor{255,
            static_cast<std::uint8_t>(std::lround(currentColor.r * 255)),
            static_cast<std::uint8_t>(std::lround(currentColor.g * 255)),
            static_cast<std::uint8_t>(std::lround(currentColor.b * 255))};
        ColorPicker picker;
        name(picker, text(IDS_COLOR));
        picker.IsAlphaEnabled(false);
        picker.Color(initialColor);
        Border swatch;
        swatch.Width(22);
        swatch.Height(22);
        swatch.CornerRadius({4, 4, 4, 4});
        swatch.Background(Media::SolidColorBrush(initialColor));
        picker.ColorChanged([this, weakSwatch = make_weak(swatch)](auto&&, auto const& args) {
            const auto color = args.NewColor();
            if (edit([&](Preset& p) {
                    for (auto& l : p.layers) {
                        l.color.r = color.R / 255.f;
                        l.color.g = color.G / 255.f;
                        l.color.b = color.B / 255.f;
                    }
                }))
                if (const auto swatch = weakSwatch.get())
                    swatch.Background(Media::SolidColorBrush(color));
        });
        Flyout colorFlyout;
        colorFlyout.Content(picker);
        DropDownButton chooseColor;
        auto colorCaption = stack(12);
        colorCaption.Orientation(Orientation::Horizontal);
        colorCaption.Children().Append(swatch);
        colorCaption.Children().Append(label(text(IDS_CHOOSE_COLOR)));
        chooseColor.Content(colorCaption);
        chooseColor.Flyout(colorFlyout);
        name(chooseColor, text(IDS_CHOOSE_COLOR));
        quick.Children().Append(chooseColor);
        quick.Children().Append(numericPreset(IDS_OPACITY_PERCENT, currentColor.a * 100, 0, 100,
            [](Preset& p, float v) { for (auto& l : p.layers) l.color.a = v / 100; }, 1, L"%"));
        fields.Children().Append(quick);
        Expander advanced;
        advanced.Header(box_value(text(IDS_ADVANCED_LAYERS)));
        advanced.IsExpanded(advancedLayers);
        quick.Visibility(advancedLayers ? Visibility::Collapsed : Visibility::Visible);
        advanced.Expanding([this](auto&&, auto&&) {
            if (!advancedLayers) {
                advancedLayers = true;
                render();
            }
        });
        advanced.Collapsed([this](auto&&, auto&&) {
            if (advancedLayers) {
                advancedLayers = false;
                render();
            }
        });
        auto advancedFields = stack(16);
        ComboBox layers;
        layers.Header(box_value(text(IDS_LAYER)));
        name(layers, text(IDS_LAYER));
        for (std::size_t i = 0; i < editing.layers.size(); ++i)
            layers.Items().Append(box_value(to_hstring(std::to_string(i + 1))));
        layers.SelectedIndex(selectedLayer);
        layers.SelectionChanged([this](auto const& sender, auto&&) {
            const auto index = sender.template as<ComboBox>().SelectedIndex();
            if (index >= 0) {
                selectedLayer = index;
                render();
            }
        });
        advancedFields.Children().Append(layers);
        auto layerActions = stack(8);
        layerActions.Orientation(Orientation::Horizontal);
        for (const auto [kind, id] : std::array<std::pair<Shape, UINT>, 3>{
                 {{Shape::dot, IDS_ADD_DOT}, {Shape::line, IDS_ADD_LINE}, {Shape::ring, IDS_ADD_RING}}})
            layerActions.Children().Append(button(text(id), [this, kind] {
                edit(
                    [&](Preset& p) {
                        Layer layer;
                        layer.shape = kind;
                        layer.endX = 10;
                        p.layers.push_back(layer);
                    },
                    true);
            }));
        layerActions.Children().Append(button(text(IDS_IMPORT_PNG), [this] { importPng(); }));
        layerActions.Children().Append(button(text(IDS_REMOVE_LAYER), [this] {
            if (editing.layers.size() > 1) {
                const auto index = selectedLayer;
                selectedLayer = 0;
                edit([index](Preset& p) { p.layers.erase(p.layers.begin() + index); }, true);
            }
        }));
        advancedFields.Children().Append(wrapControls(layerActions));
        Expander cross;
        cross.Header(box_value(text(IDS_CREATE_CROSS)));
        auto crossControls = stack(8);
        for (const bool gap : {true, false}) {
            NumberBox field;
            field.Header(box_value(text(gap ? IDS_GAP : IDS_ARM_LENGTH)));
            name(field, text(gap ? IDS_GAP : IDS_ARM_LENGTH));
            field.Minimum(gap ? 0 : 1);
            field.Maximum(40);
            field.Value(gap ? crossGap : crossLength);
            field.ValueChanged([this, gap](auto const& sender, auto&&) {
                const auto value = sender.template as<NumberBox>().Value();
                if (std::isfinite(value))
                    (gap ? crossGap : crossLength) = static_cast<float>(value);
            });
            crossControls.Children().Append(field);
        }
        crossControls.Children().Append(button(text(IDS_CREATE_CROSS), [this] {
            selectedLayer = 0;
            edit(
                [&](Preset& p) {
                    const auto style = p.layers.front();
                    const float g = crossGap, e = g + crossLength;
                    p.layers.clear();
                    for (const auto points : std::array<std::array<float, 4>, 4>{
                             {{-e, 0, -g, 0}, {g, 0, e, 0}, {0, -e, 0, -g}, {0, g, 0, e}}}) {
                        Layer line;
                        line.shape = Shape::line;
                        line.x = points[0];
                        line.y = points[1];
                        line.endX = points[2];
                        line.endY = points[3];
                        line.thickness = style.thickness;
                        line.outline = style.outline;
                        line.color = style.color;
                        p.layers.push_back(line);
                    }
                },
                true);
        }));
        cross.Content(crossControls);
        advancedFields.Children().Append(cross);
        const auto numeric = [this](UINT title, float value, float minimum, float maximum,
                                    std::function<void(Layer&, float)> assign) {
            NumberBox field;
            field.Header(box_value(text(title)));
            name(field, text(title));
            field.Minimum(minimum);
            field.Maximum(maximum);
            field.Value(value);
            field.SmallChange(.5);
            field.ValueChanged([this, assign = std::move(assign),
                                reverting = false](auto const& sender, auto const& args) mutable {
                if (reverting)
                    return;
                const auto value = sender.template as<NumberBox>().Value();
                if (!std::isfinite(value) ||
                    !edit([&](Preset& p) { assign(p.layers[selectedLayer], static_cast<float>(value)); })) {
                    reverting = true;
                    sender.template as<NumberBox>().Value(args.OldValue());
                    reverting = false;
                    error("Valore non valido: ripristinato il valore precedente.");
                }
            });
            return field;
        };
        const auto& l = editing.layers[selectedLayer];
        auto geometry = stack(8);
        geometry.Orientation(Orientation::Horizontal);
        geometry.Children().Append(numeric(IDS_POSITION_X, l.x, -96, 96, [](Layer& a, float v) { a.x = v; }));
        geometry.Children().Append(numeric(IDS_POSITION_Y, l.y, -96, 96, [](Layer& a, float v) { a.y = v; }));
        if (l.shape == Shape::png) {
            geometry.Children().Append(
                numeric(IDS_WIDTH, l.width, .5f, 192, [](Layer& a, float v) { a.width = v; }));
            geometry.Children().Append(
                numeric(IDS_HEIGHT, l.height, .5f, 192, [](Layer& a, float v) { a.height = v; }));
            ToggleSwitch filter;
            filter.Header(box_value(text(IDS_NEAREST)));
            name(filter, text(IDS_NEAREST));
            filter.IsOn(l.nearest);
            filter.Toggled([this](auto const& sender, auto&&) {
                const auto nearest = sender.template as<ToggleSwitch>().IsOn();
                edit([&](Preset& p) { p.layers[selectedLayer].nearest = nearest; });
            });
            advancedFields.Children().Append(filter);
        } else if (l.shape == Shape::line) {
            geometry.Children().Append(
                numeric(IDS_END_X, l.endX, -96, 96, [](Layer& a, float v) { a.endX = v; }));
            geometry.Children().Append(
                numeric(IDS_END_Y, l.endY, -96, 96, [](Layer& a, float v) { a.endY = v; }));
        } else
            geometry.Children().Append(
                numeric(IDS_RADIUS, l.radius, .5f, 96, [](Layer& a, float v) { a.radius = v; }));
        if (l.shape != Shape::png) {
            geometry.Children().Append(
                numeric(IDS_THICKNESS, l.thickness, .5f, 16, [](Layer& a, float v) { a.thickness = v; }));
            geometry.Children().Append(
                numeric(IDS_OUTLINE, l.outline, 0, 8, [](Layer& a, float v) { a.outline = v; }));
        }
        advancedFields.Children().Append(wrapControls(geometry));
        advancedFields.Children().Append(
            numeric(IDS_ROTATION, l.rotation, -360, 360, [](Layer& a, float v) { a.rotation = v; }));
        auto layerColors = stack(8);
        layerColors.Orientation(Orientation::Horizontal);
        if (l.shape != Shape::png) {
            layerColors.Children().Append(
                numeric(IDS_RED, l.color.r, 0, 1, [](Layer& a, float v) { a.color.r = v; }));
            layerColors.Children().Append(
                numeric(IDS_GREEN, l.color.g, 0, 1, [](Layer& a, float v) { a.color.g = v; }));
            layerColors.Children().Append(
                numeric(IDS_BLUE, l.color.b, 0, 1, [](Layer& a, float v) { a.color.b = v; }));
        }
        layerColors.Children().Append(
            numeric(IDS_OPACITY, l.color.a, 0, 1, [](Layer& a, float v) { a.color.a = v; }));
        advancedFields.Children().Append(wrapControls(layerColors));
        advancedFields.Children().Append(provenance);
        advanced.Content(advancedFields);
        fields.Children().Append(advanced);
        auto canvas = panel(inspector);
        canvas.VerticalAlignment(VerticalAlignment::Top);
        page.Children().Append(withInspector(fields, canvas));
        if (smokeTest && editing.id == "type-dot") {
            static bool checked = false;
            if (!checked) {
                checked = true;
                const auto original = editing;
                const auto originalUndo = undo, originalRedo = redo;
                const auto originalDirty = editorDirty;
                const auto originalFeedback = feedback.Text();
                const auto row = dimensionControls.Children().GetAt(0).as<StackPanel>().Children().GetAt(1).as<Grid>();
                const auto slider = row.Children().GetAt(0).as<Slider>();
                const auto number = row.Children().GetAt(1).as<NumberBox>();
                const auto require = [](bool passed) {
                    if (!passed) {
                        std::fputs("Editor slider synchronization check failed\n", stderr);
                        std::exit(1);
                    }
                };
                const auto oldPreview = editorPreview.Source();
                slider.Value(40);
                require(number.Value() == 40 && (editing.layers.front().radius * 2) == 40 &&
                        editorPreview.Source() != oldPreview && undo.size() == originalUndo.size() + 1);
                number.Value(8);
                require(slider.Value() == 8 && (editing.layers.front().radius * 2) == 8 &&
                        undo.size() == originalUndo.size() + 2);
                number.Value(std::numeric_limits<double>::quiet_NaN());
                require(number.Value() == 8 && slider.Value() == 8 && (editing.layers.front().radius * 2) == 8);
                picker.Color({255, 0, 255, 255});
                require(editing.layers.front().color.r == 0 && editing.layers.front().color.g == 1);
                editing = original;
                undo = originalUndo;
                redo = originalRedo;
                editorDirty = originalDirty;
                feedback.Text(originalFeedback);
                std::puts("Editor sliders passed: slider/number synchronization, live preview, invalid input and color picker.");
                render();
            }
        }
    }
};
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    try {
        soakTest = std::wstring_view(arguments) == L"--soak-test";
        designPreview = std::wstring_view(arguments) == L"--design-preview";
        smokeTest = designPreview || soakTest || std::wstring_view(arguments) == L"--smoke-test";
        init_apartment(apartment_type::single_threaded);
        if (smokeTest)
            useTestDataDirectory(newId());
        check_hresult(SetCurrentProcessExplicitAppUserModelID(L"CrosshairNative.Settings"));
        const auto mutex = CreateMutexW(
            nullptr, FALSE,
            (L"Local\\" + ipc::pipeName().substr(9) + (smokeTest ? L".Settings.Smoke" : L".Settings"))
                .c_str());
        if (!mutex)
            return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            EnumWindows(
                [](HWND w, LPARAM) -> BOOL {
                    if (GetPropW(w, L"CrosshairNative.Settings")) {
                        ShowWindow(w, SW_RESTORE);
                        SetForegroundWindow(w);
                        return FALSE;
                    }
                    return TRUE;
                },
                0);
            CloseHandle(mutex);
            return 0;
        }
        Application::Start([](auto&&) {
            try {
                make<SettingsApp>();
            } catch (const hresult_error& e) {
                std::fprintf(stderr, "Constructor HRESULT %08x: %s\n", static_cast<unsigned>(e.code().value),
                             to_string(e.message()).c_str());
                std::fflush(stderr);
                throw;
            }
        });
        CloseHandle(mutex);
        return 0;
    } catch (const hresult_error& e) {
        MessageBoxW(nullptr, e.message().c_str(), product::name.data(), MB_OK | MB_ICONERROR);
        return 1;
    } catch (const std::exception& e) {
        MessageBoxW(nullptr, to_hstring(e.what()).c_str(), product::name.data(), MB_OK | MB_ICONERROR);
        return 1;
    }
}
