# Architettura

C++20, due processi. `CrosshairNative.Engine.exe` Win32 possiede Raw Input, overlay, tray, stato e scritture persistenti. `CrosshairNative.Settings.exe` WinUI 3/C++/WinRT presenta profili, onboarding, libreria e editor. Chiudere la UI ferma il suo timer e termina quel processo. Nessun browser, database, watchdog o servizio.

Il core contiene matching scan code/E0/E1 e Mouse 3–5, politica modificatori, conflitti, edge detection, priorità e fallback. Raw Input ha una sola registrazione nel motore; nessuna soppressione o reiniezione. Al cambio focus lo stato dedotto si invalida e i tasti mantenuti entrano in quarantena fino al rilascio.

WinEvent fuori processo coalescenti aggiornano eleggibilità e centro client in pixel fisici. D2D software/WIC produce PBGRA 257×257. Il centro del raster è 128,5; overlay PMv2 non attivabile, layered/transparent/topmost, assente da taskbar e Alt+Tab. Surface assegnate/fallback vengono preparate prima del commit: il cambio slot usa solo cache e UpdateLayeredWindow, senza disco. Lo stesso renderer produce anteprime ed esportazioni di test.

IPC: named pipe locale con ACL del SID di logon, rifiuto client remoti e singola istanza. Frame uint32 little endian + JSON UTF-8, massimo 128 KiB; schema protocollo 1, request ID, risultato/errore e revisione configurazione. Client ACK evita di perdere risposte prima della disconnessione. I/O overlapped con timeout/cancellazione; comandi serializzati sul thread motore. Nessun comando shell, percorso arbitrario o lettura memoria del gioco. UI invia comandi in coda limitata e richiede snapshot mentre aperta (800 ms).

Comandi: GetState/ListTargets/ListProfiles, SaveProfile/SwitchProfile, StartCapture/CancelCapture, StartPreview/EndTest, SetPaused/Shutdown, SavePreset/ImportAsset, SetFavorite/MarkRecent/SetTheme/SetAutoStart/SetDiagnostics/GetDiagnostics/RecoverProfile. Le modifiche hanno ACK; SaveProfile e SwitchProfile controllano configRevision. Una copia readonly del backup richiede recupero esplicito.

Persistenza JSON schema 1: active.json è la fonte del profilo attivo, copie profile-ID.json mantengono i profili; settings.json mantiene preferenze italiane/tema/favoriti/recenti/avvio/diagnostica. Preset e PNG personali restano separati dal catalogo installato. Scritture con temporaneo CREATE_NEW, flush e ReplaceFile con backup; niente scritture a ogni input. Una sequenza che tocca più file non è una transazione globale: ogni file resta atomico.

La galleria carica metadati, virtualizza GridView e genera solo miniature realizzate, cache massima 48. Editor: 16 livelli, undo/redo limitato 64, PNG statici WIC, copie nuove. Provenienza dichiarata viene conservata in JSON, senza scaricare URL.

.crosshairpack è un envelope JSON UTF-8 con manifest, profilo, preset e PNG Base64, non un archivio ZIP. Limiti di dimensione/profondità e riferimenti validati, ID rimappati; file personali mai sovrascritti. La UI scrive solo staging temporaneo e file export scelti dall’utente; il motore installa i contenuti persistenti.
