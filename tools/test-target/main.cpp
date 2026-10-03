#include "core/product.hpp"
#include <windows.h>
#include <filesystem>
#include <string>

namespace {
unsigned clicks = 0, centerClicks = 0, keyDowns = 0;
bool started = false, profileTest = false;
std::wstring notice;
void beginTest(HWND window) {
    if (const auto engine = FindWindowW(crosshair::product::engineClass.data(), nullptr)) {
        PostMessageW(engine, WM_COMMAND, profileTest ? 7 : 5, 0);
        started = true;
        notice =
            profileTest
                ? L"Usa i tasti appena registrati nel tuo profilo. Chiudi questa prova per tornare al gioco."
                : L"Prova avviata. Premi 1, 2 e 3, poi torna a 1 e clicca il punto centrale.";
    } else {
        wchar_t module[32768]{};
        GetModuleFileNameW(nullptr, module, 32768);
        const auto executable =
            (std::filesystem::path(module).parent_path() / L"CrosshairNative.Engine.exe").wstring();
        auto command = L"\"" + executable + L"\" --test";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                           &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            started = true;
            notice = L"Motore avviato. Clicca questa finestra e premi 1, 2 e 3.";
        } else
            notice = L"Motore non avviato. Compila il progetto e verifica che Engine.exe sia nella stessa "
                     L"cartella.";
    }
    SetFocus(window);
    InvalidateRect(window, nullptr, TRUE);
}
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_CREATE:
        CreateWindowExW(0, L"BUTTON",
                        profileTest ? L"Prova tasti del profilo" : L"Avvia prova (tasti 1, 2, 3)",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 20, 20, 300, 44, window,
                        reinterpret_cast<HMENU>(1), GetModuleHandleW(nullptr), nullptr);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == 1)
            beginTest(window);
        return 0;
    case WM_LBUTTONDOWN: {
        ++clicks;
        RECT r{};
        GetClientRect(window, &r);
        const int x = static_cast<short>(LOWORD(l)), y = static_cast<short>(HIWORD(l));
        if (abs(x - r.right / 2) <= 3 && abs(y - r.bottom / 2) <= 3)
            ++centerClicks;
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_KEYDOWN:
        ++keyDowns;
        if (w == VK_RETURN && !started)
            beginTest(window);
        if (w == VK_F11) {
            const auto style = GetWindowLongPtrW(window, GWL_STYLE);
            SetWindowLongPtrW(window, GWL_STYLE,
                              (style & WS_CAPTION) ? WS_POPUP | WS_THICKFRAME : WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_SIZE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_DPICHANGED: {
        const auto* r = reinterpret_cast<RECT*>(l);
        SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        RECT r{};
        GetClientRect(window, &r);
        FillRect(dc, &r, GetSysColorBrush(COLOR_WINDOW));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        const auto font = CreateFontW(-MulDiv(17, GetDpiForWindow(window), 96), 0, 0, 0, FW_NORMAL, FALSE,
                                      FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        const auto previous = SelectObject(dc, font);
        const std::wstring instructions =
            profileTest
                ? L"1. Premi il pulsante qui sopra.\n2. Usa i tasti che hai registrato nelle "
                  L"impostazioni.\n3. Clicca sul mirino: «Clic al centro» deve aumentare (con offset "
                  L"zero).\n4. Tieni premuto Alt, premi Tab e rilascia: tornando alle impostazioni il mirino "
                  L"deve sparire."
                : L"1. Premi «Avvia prova» qui sopra (oppure Invio).\n2. Premi 1 = punto, 2 = croce, 3 = "
                  L"anello.\n3. Premi 1 e clicca il punto al centro: «Clic al centro» deve aumentare.\n4. "
                  L"Tieni premuto Alt, premi Tab e rilascia: su un’altra finestra il mirino deve sparire.";
        RECT textArea{20, 82, r.right - 20, 260};
        DrawTextW(dc, instructions.c_str(), -1, &textArea, DT_WORDBREAK | DT_NOPREFIX);
        const auto status = L"Clic al centro: " + std::to_wstring(centerClicks) + L"     Clic totali: " +
                            std::to_wstring(clicks) + L"\nTasti ricevuti dal bersaglio: " +
                            std::to_wstring(keyDowns) + L"     DPI: " +
                            std::to_wstring(GetDpiForWindow(window)) + L"\n\n" + notice +
                            L"\n\nProva temporanea: nessuna modifica ai tuoi tasti personali.\nSposta o "
                            L"ridimensiona la finestra per controllare la centratura. F11 cambia i bordi.";
        RECT statusArea{20, r.bottom - 170, r.right - 20, r.bottom - 10};
        DrawTextW(dc, status.c_str(), -1, &statusArea, DT_WORDBREAK | DT_NOPREFIX);
        const int x = r.right / 2, y = r.bottom / 2;
        MoveToEx(dc, x - 20, y, nullptr);
        LineTo(dc, x - 12, y);
        MoveToEx(dc, x + 12, y, nullptr);
        LineTo(dc, x + 20, y);
        MoveToEx(dc, x, y - 20, nullptr);
        LineTo(dc, x, y - 12);
        MoveToEx(dc, x, y + 12, nullptr);
        LineTo(dc, x, y + 20);
        SelectObject(dc, previous);
        DeleteObject(font);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (started)
            if (const auto engine = FindWindowW(crosshair::product::engineClass.data(), nullptr))
                PostMessageW(engine, WM_COMMAND, 6, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, w, l);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int show) {
    profileTest = std::wstring_view(arguments) == L"--profile";
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = windowProc;
    wc.lpszClassName = crosshair::product::targetClass.data();
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc))
        return 1;
    const auto window = CreateWindowExW(0, wc.lpszClassName, L"Crosshair Native — Prova semplice",
                                        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                        1000, 740, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 1;
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
