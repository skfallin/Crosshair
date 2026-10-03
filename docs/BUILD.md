# Build e pacchetto

Ambiente verificato: Windows 11 x64 build 26200, PowerShell 7, Visual Studio Build Tools 2022 17.14.37710.0, MSVC toolset 14.44.35207 (compiler 19.44.35229), SDK 10.0.26100.0 e CMake 3.31.6-msvc6. Prerequisiti installati con autorizzazione dell’utente. Componenti C++ desktop e CMake richiesti solo per sviluppare.

WinUI 3 usa il componente NuGet Microsoft.WindowsAppSDK.WinUI 1.8.260803003 e C++/WinRT 2.0.250303.1, versioni esatte. `apps/settings/packages.lock.json` contiene transitive e hash; `NuGet.Config` ammette solo nuget.org. Ripristino iniziale con rete, uso normale completamente locale.

```powershell
.\scripts\build-settings.ps1 -Restore
.\scripts\build.ps1 -Configuration Release
```

Lo script compila CMake, esegue sette suite CTest, genera catalogo e miniature, compila WinUI e copia motore/bersaglio/catalogo accanto alla UI. Percorsi con spazi supportati. Le variabili di ambiente duplicate Path/PATH dell’host vengono normalizzate solo nel processo figlio.

```powershell
# Solo motore, catalogo e test
.\scripts\build.ps1 -EngineOnly
# Build UI separata quando l'app precedente è aperta
.\scripts\build-settings.ps1 -OutputDirectory out/settings-next/Release
# Test UI interno: richiede sessione desktop
.\out\settings\Release\CrosshairNative.Settings.exe --smoke-test
.\out\settings\Release\CrosshairNative.Settings.exe --soak-test
# Benchmark del motore in attesa, isolato dai profili reali
.\out\build\Release\engine_tests.exe --benchmark
# Installer per utente
.\scripts\package.ps1
```

Prima di confezionare un percorso alternativo, copiarvi `CrosshairNative.Engine.exe`, `CrosshairNative.TestTarget.exe` da `out/build/Release` e la cartella `out/build/catalog`. Poi `package.ps1 -ApplicationDirectory out/settings-next/Release`.

## Installazione

Lo script usa Windows Installer COM e makecab già presenti in Windows. Produce un MSI con CAB incorporato, directory staging `app`, `inventory.json` con SHA-256 per file e `package.json` con dimensioni/hash del pacchetto. Non richiede WiX o un servizio aggiuntivo.

Installazione solo per l’utente corrente in `%LOCALAPPDATA%\Programs\CrosshairNative`; collegamento nel suo menu Start e registrazione in App installate. Nessun ALLUSERS o elevazione applicativa. Il pacchetto include CRT 14.44.35112 e runtime WinUI self-contained. Esclude simboli e file WebView2: la dipendenza NuGet di compilazione non diventa un browser nell’app. I componenti restano nella directory dell’app.

Chiudere le impostazioni e scegliere **Esci completamente** prima di aggiornare o disinstallare. Disinstallare da App installate. I dati `%LOCALAPPDATA%\CrosshairNative` sono deliberatamente conservati; la voce Run dell’app è rimossa. L’installer non abilita l’avvio automatico: crea solo una voce vuota di cui gestisce la rimozione. L’app la valorizza soltanto dopo scelta esplicita.

Questa versione MSI locale non implementa aggiornamenti automatici né upgrade fra versioni pubbliche: rimuovere la precedente prima di installare un nuovo pacchetto. Il certificato di firma non è definito. Codice originale e catalogo: PolyForm Noncommercial 1.0.0, attribuzione a skfallin; LICENSE e NOTICE sono inclusi in licenses/ nel pacchetto. Non disattivare Defender o SmartScreen.

Riferimenti del packaging: [tabelle MSI](https://learn.microsoft.com/en-us/windows/win32/msi/database-tables), [contesto per utente](https://learn.microsoft.com/en-us/windows/win32/msi/installation-context), [pacchetti senza elevazione](https://learn.microsoft.com/en-us/windows/win32/msi/word-count-summary). La CI Windows è configurata per build completa e test; non è stata eseguita su un runner remoto.
