# Compatibilità verificata

| Ambiente/scenario | Stato al 29 settembre 2026 |
|---|---|
| Windows 11 x64 build 26200 | Build e test locali eseguiti |
| TestTarget, tasti 1/2/3 | Cambio mirino confermato dall’utente |
| Clic sul pixel centrale visibile | Contatore del bersaglio aumenta, confermato dall’utente |
| Fortnite, profilo selezionato dalla UI | Mirino sovrapposto confermato dall’utente |
| Versione Fortnite/modalità grafica precisa | Non raccolte; era stata indicata finestra/borderless |
| Alt+Tab, minimize, lock | Gestione implementata; conferma manuale finale mancante |
| DPI misti, coordinate negative, monitor scollegato | Implementati; prova hardware mancante |
| Schermo intero esclusivo | Non garantito dal backend standard |
| Game Bar | Non inclusa e non testata |
| Windows 10, ARM64, altre build/driver | Non dichiarati verificati |
| Lettore schermo/contrasto elevato/DPI UI 200% | Controlli nativi/etichette; verifica assistiva manuale mancante |

Identificazione del gioco tramite percorso e classe finestra. Nessuna firma di processo o lista fissa di titoli da aggiornare. Nessuna elevazione automatica se l’accesso ai metadati è negato. L’overlay viene mostrato solo sul bersaglio eleggibile in primo piano.

La riuscita su una configurazione Fortnite non equivale ad autorizzazione del gioco, dell’anti-cheat o di un torneo. Nessuna certificazione dichiarata. L’app non analizza inventario, HUD, schermate o memoria del gioco.
