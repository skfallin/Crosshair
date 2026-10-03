# Crosshair Native

App nativa offline per Windows 11 x64. Build locale **0.1.0 di sviluppo, non firmata**; nome provvisorio. Overlay Win32 e impostazioni WinUI 3 sono processi separati.

Sono implementati cinque slot configurabili, tastiera e Mouse 3–5, pausa/nascondi, associazione alla finestra del gioco, profili, procedura guidata, 8 tipi di mirino personalizzabili con anteprima dal vivo e salvataggio personale, ricerca/famiglie/preferiti/recenti, editor fino a 16 livelli, PNG trasparenti, import/export e diagnostica facoltativa. Il mirino segue il tasto associato allo slot: non legge l’inventario del gioco.

## Uso

1. Apri **Crosshair Native** dal menu Start dopo l’installazione, oppure `CrosshairNative.Settings.exe` nella cartella dell’app.
2. In **Impostazioni**, aggiorna l’elenco finestre e seleziona il gioco. In **Slot e tasti**, registra i tuoi tasti. Ogni slot mostra il mirino assegnato: premi **Scegli mirino**, seleziona una miniatura e conferma con **Usa questo mirino**. Puoi cercare per nome, famiglia o tag.
3. Premi **Salva profilo**, poi torna al gioco in finestra o senza bordi.
4. La chiusura delle impostazioni lascia attivo il motore. Per fermarlo usa **Esci completamente** dalla tray.

In **Libreria → I tuoi mirini**, seleziona un mirino e premi **Modifica**. **Salva modifiche** aggiorna il mirino mantenendo le assegnazioni agli slot e ai profili; **Salva come nuovo mirino** crea una copia personale. I predefiniti possono essere personalizzati e salvati come nuovi mirini. Per esportare anche i PNG assegna il mirino a uno slot ed esporta il profilo come `.crosshairpack`. L’importazione mostra un riepilogo e crea nuovi ID; collega nuovamente la finestra del gioco prima di salvare il profilo importato.

L’interfaccia usa una barra laterale, schede con anteprime e un tema antracite/lime. Il **Mirino predefinito** è nella sesta scheda di **Slot e tasti**; la rimozione di un tasto è nel menu **···** della scheda. In **Libreria**, apri **Importa ed esporta** per gli scambi di file. Temi chiaro/scuro/sistema in **Impostazioni**. Direzione visiva e verifiche in [DESIGN](docs/DESIGN.md).

## Build

Da PowerShell 7 nella cartella del repository:

```powershell
.\scripts\build-settings.ps1 -Restore
.\scripts\build.ps1
.\out\settings\Release\CrosshairNative.Settings.exe
.\scripts\package.ps1
```

Output completo: `out/settings/Release`. Installer per utente: `out/package/<data-ora>/CrosshairNative-0.1.0-dev-x64.msi`, con inventario SHA-256. Il pacchetto incorpora WinUI e CRT; l’app non richiede browser, .NET, Game Bar o toolchain installata. Dettagli in [BUILD](docs/BUILD.md).

## Verifiche e limiti

Il 29 settembre 2026: sette suite native superate, smoke test WinUI e 120 attraversamenti delle schermate/temi senza crash. L’utente ha confermato cambio 1/2/3 e clic sul punto nel bersaglio, poi comparsa del mirino su Fortnite tramite il pannello.

Restano da verificare manualmente Alt+Tab/minimizzazione/blocco, monitor con DPI diversi, accessibilità con lettore schermo e mouse ad alta frequenza. Latenza evento→backend e impatto sui frame time del gioco non sono ancora misurati. Nessuna garanzia per fullscreen esclusivo; Game Bar non è inclusa.

I risultati dettagliati sono in [VALIDATION](docs/VALIDATION.md), [TESTING](docs/TESTING.md), [PERFORMANCE](docs/PERFORMANCE.md) e [COMPATIBILITY](docs/COMPATIBILITY.md). La build di sviluppo non viene presentata come release 1.0 certificata.

Dati personali in `%LOCALAPPDATA%\CrosshairNative\`, conservati dalla disinstallazione. Nessun account, rete, telemetria, injection o cattura del gioco. [Privacy](docs/PRIVACY.md).

## Licenza

Il codice originale, la documentazione, le icone del progetto e il catalogo generato sono disponibili sotto [PolyForm Noncommercial 1.0.0](LICENSE), con attribuzione a [skfallin](https://github.com/skfallin) in [NOTICE](NOTICE).

Sono consentiti uso, modifiche e condivisione per gli scopi non commerciali definiti dalla licenza. Non sono concessi vendita, rivendita delle versioni modificate o sfruttamento commerciale del software. Quando condividi una copia, conserva la licenza e l'avviso obbligatorio. Il testo completo della licenza prevale su questo riepilogo; include gli usi consentiti delle organizzazioni non commerciali.

Il progetto è a sorgente disponibile con restrizione non commerciale. Le dipendenze Microsoft mantengono le proprie licenze; i PNG importati dagli utenti restano soggetti ai diritti dei rispettivi titolari. La pubblicazione del codice non certifica la build per una release: restano i controlli della [checklist](docs/RELEASE_CHECKLIST.md) e le eventuali autorizzazioni del gioco.
