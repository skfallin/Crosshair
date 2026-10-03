# Checklist release

## Implementato/verificato localmente

- [x] C++20 motore Win32 e UI WinUI 3 separati; build Release locale.
- [x] Core, renderer, catalogo, persistenza, IPC, pack e motore reale: sette suite.
- [x] 323 geometrie originali, report/tavola, ID stabili e deduplicazione.
- [x] Profili/onboarding/editor/PNG/import-export/tema/preferenze/diagnostica.
- [x] Corruzione: originali preservati e recupero esplicito testato.
- [x] Conferme manuali cambio mirino, clic centrale e comparsa su Fortnite.
- [x] Smoke WinUI senza DLL WebView2.
- [x] 120 attraversamenti UI e baseline motore in attesa su 60 s.
- [x] Script per MSI per utente, runtime app-local e inventario SHA-256.
- [x] CI configurata per compilare motore, UI e test; nessun esito remoto inventato.

## Gate ancora aperti per pubblicazione

- [x] Installazione/disinstallazione MSI e smoke installato, hash file e preservazione del profilo verificati (VALIDATION).
- [ ] Alt+Tab/minimize/lock, focus mantenuto, DPI misti e coordinate negative.
- [ ] Accessibilità assistiva, alto contrasto, tastiera e DPI UI elevati.
- [ ] CPU con overlay statico visibile, p95 evento→backend, frame time Fortnite acceso/spento.
- [ ] Soak lungo, input mouse ad alta frequenza, crash/restart e perdita IPC.
- [ ] CI remota e prova su Windows pulito senza toolchain.
- [x] Licenza originale PolyForm Noncommercial 1.0.0 e attribuzione skfallin in LICENSE/NOTICE, incluse dal packaging.
- [ ] Verifica nome e avvisi completi di redistribuzione dei componenti terzi.
- [ ] Decidere/fare firma e procedure di distribuzione pubblica. Mai dichiarare firmata questa build locale.
- [ ] Eventuali autorizzazioni gioco/torneo separate dalla prova tecnica.

Game Bar non è requisito della build standard e non viene distribuita.
