# Dipendenze e avvisi

Inventario esatto di compilazione con versioni/hash: apps/settings/packages.lock.json. Il pacchetto include una copia, avvisi e licenze trovati nelle radici NuGet e inventory.json con SHA-256 dei file distribuiti.

| Componente | Versione | Impiego |
|---|---|---|
| MSVC toolset | 14.44.35207 | Build C++20 |
| MSVC CRT x64 | 14.44.35112 | DLL app-local nel pacchetto |
| Windows SDK | 10.0.26100.0 | Win32, D2D, WIC, WinRT di sistema |
| CMake | 3.31.6-msvc6 | Build core/motore/test |
| C++/WinRT | 2.0.250303.1 | Proiezioni C++ |
| WindowsAppSDK.WinUI | 1.8.260803003 | UI nativa |
| WindowsAppSDK.Foundation | 1.8.260803002 | Runtime app-local |
| WindowsAppSDK.InteractiveExperiences | 1.8.260708001 | Dipendenza WinUI |
| WindowsAppSDK.Base | 1.8.251216001 | Build/transitive |
| WebView2 SDK | 1.0.3179.45 | Solo dipendenza metadata del pacchetto WinUI; file esclusi dal prodotto |
| SDK BuildTools | 10.0.26100.4654 | Strumenti build |
| SDK BuildTools.MSIX | 1.7.20250829.1 | Strumenti transitive; nessun MSIX/Game Bar prodotto |

Nessuna dipendenza runtime da browser/Chromium, .NET, Qt o server. Niente installer VC globale: CRT nella cartella dell’app. Le DLL Microsoft conservano le loro firme, che non firmano il codice originale o il pacchetto MSI.

Riferimenti: [Windows App SDK](https://github.com/microsoft/WindowsAppSDK), [C++/WinRT](https://github.com/microsoft/cppwinrt), [redistribuzione Visual C++](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files). La distribuzione dei binari richiede revisione degli avvisi completi e delle condizioni di redistribuzione applicabili alla toolchain. La licenza PolyForm Noncommercial 1.0.0 del codice originale non sostituisce queste licenze. LICENSE e NOTICE del progetto sono inclusi dal packaging insieme agli avvisi delle dipendenze.
