# Decisioni

- Backend standard prima di Game Bar: Win32 layered 257×257, D2D software/WIC PBGRA, nessun render loop. Game Bar resta una fase opzionale non avviata.
- WinUI 3 nativo separato e self-contained; dipendenza al solo componente WinUI dello SDK. WebView2 resta una dipendenza metadata imposta da NuGet, esclusa dai file finali e non usata dall’interfaccia.
- Core C++20 senza framework di test esterni; test eseguibili via CTest.
- JSON Windows.Data.Json, schema 1 con default retrocompatibili per metadati e rotazione; versioni future rifiutate. Nessun formato storico da migrare inventato.
- Active profile in active.json e copie per ID. Backup readonly finché l’utente conferma il recupero; file corrotto conservato con suffisso .corrupt.
- Catalogo deterministico di 323 geometrie, generato in build, originale e senza scraping.
- Libreria virtualizzata e cache 48 miniature; motore conserva soltanto preset assegnati/fallback.
- Pacchetto JSON/Base64 invece di ZIP: evita estrazione di percorsi, link e file eseguibili. Budget 100 MiB anche per somma dei pixel PNG decodificati; input testuale massimo 100 MiB. Importazione incrementale con nuovi ID: un errore può lasciare copie già importate, ma non modifica il profilo attivo e non sovrascrive dati.
- MSI per utente tramite capacità native di Windows. Niente framework installer o aggiornamento automatico; build locale senza firma distinta dalla pubblicazione, ancora bloccata da licenza/titolare e checklist.
