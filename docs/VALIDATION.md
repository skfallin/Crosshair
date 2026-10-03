# Report di validazione — 29 settembre 2026

Build locale di sviluppo 0.1.0, Windows 11 x64 26200. Questo report distingue implementazione, esecuzione automatica e riscontro umano.

| Fase | Consegna ed evidenza | Limiti |
|---|---|---|
| 0 | Toolchain pin, core C++20, JSON schema1, CI e documenti | CI remota non eseguita |
| 1 | Motore/tray, 3 preset iniziali, Raw Input e TestTarget; utente conferma 1/2/3 e clic centrale | Alt+Tab/DPI/hardware da completare |
| 2 | WinUI separato, IPC, onboarding e profili; utente conferma mirino su Fortnite dopo selezione/salvataggio dal pannello | Non raccolti versione/modalità precisa di Fortnite |
| 3 | 323 preset, editor 16 livelli/PNG, ricerca/favoriti/recenti, pacchetti e recupero; test automatici e smoke UI passano | Tutti i flussi con tastiera/lettore schermo non ancora verificati |
| 4 | Script MSI, runtime app-local, inventario, baseline 60s e 120 cicli UI | Budget overlay visibile/latency/game frames e release gate da chiudere |
| 5 | Non avviata | Game Bar facoltativa non inclusa |

Comandi eseguiti: build-settings.ps1 -Restore (locked); build.ps1 -EngineOnly; build-settings.ps1 -OutputDirectory out/settings-next/Release; catalog_builder tramite CTest; Settings --smoke-test; Settings --soak-test; engine_tests --benchmark; package.ps1 con percorso alternativo.

Sette suite CTest passate: core, catalog, renderer, storage, ipc, pack, engine. Report dettagliato generato in out/build/Testing/Temporary/LastTest.log. Catalogo: out/build/catalog/VALIDATION.md, 323 fingerprint unici; comparison.png esaminata. Geometria/PNG/pacchetti controllati con codice reale, non dati simulati in UI.

Smoke su staging privo dei file WebView2: uscita 0. Soak 120 viste/temi: uscita 0, cache 48; misure precise in PERFORMANCE. Il tool di automazione desktop identifica erroneamente il processo Settings come impostazioni di Windows: non è stato usato per fingere una verifica visuale o interagire con l’app sbagliata. Il riscontro umano sul collegamento Fortnite resta valido.

La pubblicazione non è avvenuta. Nessun commit/push, firma o autorizzazione del gioco inventati. Consultare RELEASE_CHECKLIST per i gate aperti.

Installazione MSI per utente riuscita (exit 0); 910 file verificati contro SHA-256, registrazione Windows Installer valida e smoke della UI installata exit 0. Disinstallazione exit 0: nessun file applicativo residuo, voce Run rimossa, active.json invariato byte per byte. Le due occorrenze WebView2 nel pacchetto sono avvisi/licenze testuali: nessuna DLL WebView2. Report out/installer-verification.json.


Build completa finale eseguita con scripts/build.ps1; sette suite ancora passate, inclusi cinque ulteriori riavvii con risposta Shutdown consumata prima dell’uscita. Formattazione clang-format, sintassi PowerShell e JSON verificati. Soak finale UI con metadati/recupero: 120 viste, uscita 0, dati in out/ui-soak-final.txt.


Pacchetto finale: out/package/20260929-124858/CrosshairNative-0.1.0-dev-x64.msi. Installato e avviato: 910 hash verificati, 0 DLL/winmd WebView2, smoke exit 0, profilo byte per byte conservato. SHA-256 19204B6F4C224F39E24AA7B9C6956EB9BC97754FCF96E7E294ED83A21F16221A. Dati in out/final-install-verification.json. Dimensioni finali: 58.919.441 byte payload, 21.643.264 byte MSI.


Verifica IPC finale della versione installata: profilo Fortnite caricato, bersaglio configurato, backend disponibile, nessun avviso, modalità prova disattivata, un solo motore e una sola UI. Binding persistenti: 0; i tasti 1/2/3 usati nel bersaglio erano temporanei. L’utente deve scegliere i propri binding in Slot e tasti prima del cambio automatico nel gioco. Avvio automatico rimasto disabilitato.


Aggiornamento selezione visiva: elenco testuale dei preset sostituito da miniatura per slot e dialogo con griglia virtualizzata condivisa con la libreria. Ricerca per nome/famiglia/tag, selezione iniziale e conferma/annullamento. Build Settings Release riuscita; smoke del dialogo (selezione, ricerca/reset, annullamento) exit 0. Controllo clang-format superato. Il motore non è stato modificato da questo aggiornamento.

# Ridisegno UI — 29 settembre 2026

Interfaccia nativa aggiornata seguendo il riferimento fornito: barra laterale, schede slot con miniature, selettore visivo, pannello di anteprima nella libreria/editor, impostazioni raggruppate e risorse scuro/chiaro/alto contrasto. Il tema ora si applica all’intera finestra e ai dialoghi. Motore e dati dei preset non modificati.

Build Settings Release senza warning. Sette suite CTest passate; clang-format e controllo spazi/conflitti superati. `--design-preview` exit 0, sette immagini reali WinUI esaminate e correzioni applicate a contrasto e icone. Finestre verificate: 1240×940 e 960×780 pixel. Selettore: selezione, filtro vuoto/reset, annullamento e nessuna modifica involontaria durante la navigazione.

Soak aggiornato: 120 viste/temi, exit 0. A 12/60/120 cicli: 174.837.760 / 178.888.704 / 179.056.640 byte privati; 1820 / 1818 / 1821 handle; cache 39. Dati in `out/ui-design-soak.txt`. Le sei coppie di colori testuali di progetto controllate hanno rapporti da 5,13:1 a 14,73:1 (`out/design-contrast.json`); non è una certificazione di tutti gli stati/accessibilità dell’app.

Pacchetto `out/package/20260929-133816/CrosshairNative-0.1.0-dev-x64.msi`, SHA-256 `570F11D6F6D8FA238C0F2C549451A1F670411A1673985A9D85C116BD39F74FC6`. L’aggiornamento MSI diretto con lo stesso ProductCode/versione è stato rifiutato da SecureRepair per hash differente (1603). Disinstallazione e nuova installazione con Windows Installer: entrambe exit 0. Nessuna policy di sicurezza modificata. 910 file installati verificati con hash, tutti i 6 file dati conservati byte per byte, registrazione prodotto valida.

Avviati motore e UI installati. Tema scuro applicato tramite API dell’app; avvio automatico già abilitato dall’utente ripristinato tramite SetAutoStart. Profilo attivo invariato, bersaglio configurato, backend disponibile e nessun avviso. Report: `out/design-install-verification.json` e `out/design-runtime-verification.json`. Restano manuali le prove con lettore schermo, alto contrasto di sistema e DPI multipli.
