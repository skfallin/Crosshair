# Privacy e sicurezza

Nessun account, endpoint di rete, telemetria, cloud, pubblicità, controllo licenza periodico o aggiornamento automatico. Nessun driver, servizio, injection, memoria di gioco, hook grafico, macro, input sintetico o screenshot del gioco. La compilazione iniziale usa NuGet; il programma compilato non richiede rete.

Raw Input osserva tastiera e Mouse 3–5; solo binding nel bersaglio o registrazione esplicita producono effetti. Si conserva solo lo stato transitorio necessario a rilasci/quarantena, mai testo digitato, movimenti o cronologia.

Dati in %LOCALAPPDATA%/CrosshairNative: percorso dell’eseguibile e classe della finestra bersaglio, profili, binding, etichette, offset, preset, PNG, metadati di provenienza, preferiti/recenti, tema/avvio e preferenza diagnostica. Nessun titolo finestra, PID/HWND persistito, hardware ID o pressione runtime. Prima di condividere un export, considerare che un profilo contiene il percorso dell’eseguibile; l’importatore lo azzera sul PC destinatario.

Diagnostica opt-in, predefinita spenta: eventi tecnici fissi in diagnostics/engine.log, massimo circa 64 KiB con una rotazione .1. Nessun input o titolo nel log. Il report su richiesta mostra versione, architettura/backend, memoria privata, handle, cache e stato visibilità; l’utente ne vede il contenuto prima di scegliere dove salvarlo. Non viene inviato automaticamente.

PNG statici: 16 MiB e 2048×2048, un solo frame, decoder WIC PNG. JSON profili/preset 64 KiB e profondità 8. Pacchetti massimo 100 MiB, 2000 elementi, ID ASCII vincolati, riferimenti completi, nessun percorso di estrazione. Link/reparse point nelle destinazioni rifiutati. Staging identificato da nuovi GUID, file CREATE_NEW, nessuna sovrascrittura personale.

La named pipe accetta solo la sessione di logon locale e comandi limitati. Non è un confine di sicurezza contro altro codice già eseguito dall’utente. App eseguita come utente standard; non viene elevata per interrogare un gioco.
