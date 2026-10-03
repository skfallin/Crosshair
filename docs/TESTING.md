# Test

## Automatici

`scripts/build.ps1 -EngineOnly` compila ed esegue sette suite CTest:

1. core: 49 controlli originari su matching, priorità/conflitti/modificatori, idempotenza, edge/focus/quarantena, limiti.
2. catalog: 8 tipi unici ignorando colore/nome/ordine, validazione, round trip e render.
3. renderer: alpha premoltiplicato, determinismo, simmetria; tolleranza alpha 4/255 e centro entro 0,02 pixel.
4. storage: profili/preset, tipi/limiti/schema futuro, replace atomico, backup readonly, preservazione dell’originale.
5. ipc: pipe locale, risposte ripetute di 20 KB, frame e richieste non validi, shutdown.
6. pack: round trip geometria+PNG/alpha, riferimenti e ID ostili, duplicati, limiti/profondità/dimensioni.
7. engine: processo reale --self-test con pipe/mutex/directory temporanea esclusivi; comandi, preset personale, profili, revisioni obsolete, preferenze, eliminazione dei mirini personali con protezione dei predefiniti e dei riferimenti nei profili, riavvio, corruzione e recupero esplicito che conserva l’originale, shutdown.

Il test motore non invia input sintetico. Usa %TEMP%/CrosshairNative.Test.<GUID>, non i profili reali. Cleanup solo dopo verifica del percorso e assenza di reparse point.

Smoke UI: `CrosshairNative.Settings.exe --smoke-test`, sei viste e tre temi, chiusura automatica. Soak: `--soak-test`, 120 viste e campioni memoria/handle/cache. Entrambi saltano IPC e avvio del motore reale; verificano costruzione e cicli UI, non equivalgono a test manuali di tutti i pulsanti.

Lo smoke verifica anche la separazione Predefiniti/I tuoi mirini e l'assenza di selezioni residue nelle categorie vuote, oltre alla sincronizzazione bidirezionale slider/valore numerico, l'aggiornamento dell'anteprima, il ripristino degli input non validi e il selettore colore, senza salvare modifiche ai mirini personali.

## Manuali

TestTarget da cartella app: **Avvia prova**, 1 punto / 2 croce / 3 anello. Clic sul centro conta anche attraverso il pixel visibile. F11 commuta bordi. Chiudere il bersaglio ripristina il profilo personale. Nella procedura guidata, la prova configurata usa i propri binding/preset.

Checklist ancora da completare: tenere un tasto, Shift/modificatori, Mouse 3–5, pausa/nascondi/ripresa; Alt+Tab, riduzione a icona e blocco; resize/move/DPI misti/negative coordinates; UI da sola tastiera, lettore schermo e contrasto elevato; chiusura UI con modifiche; seconda apertura; uso prolungato e mouse ad alta frequenza. Nessun input sintetico a Fortnite.

## Dati e recupero

Profili in %LOCALAPPDATA%/CrosshairNative/profiles. Un active.json corrotto carica .bak readonly; se anche il backup è invalido, profilo iniziale readonly. In **Impostazioni → Ripristina profilo recuperato** si può confermare il recupero: l’originale viene spostato in active.json.corrupt.<numero>, poi si salva il profilo recuperato. Il backup e gli altri profili non vengono cancellati.

Preferenze illeggibili vengono conservate e bloccano le riscritture; per recuperarle manualmente, chiudere UI/motore e conservare una copia di settings.json prima di ripristinare il relativo .bak. Nessun reset globale automatico.

Pacchetti: validazione prima dell’import, riepilogo, nuovi ID e bersaglio azzerato. Se un errore avviene durante l’installazione incrementale, gli elementi già aggiunti possono restare nella libreria; il profilo attivo resta invariato. Per ripetere si generano altri ID, non si sovrascrivono file.

Per installazione/disinstallazione vedere BUILD; per prove del gioco e hardware vedere COMPATIBILITY; per misure e limiti vedere PERFORMANCE.

## Selezione visiva degli slot
Lo smoke WinUI verifica anche apertura del selettore a miniature, selezione, ricerca senza risultati, ripristino dell’elenco e annullamento senza assegnazione. Gli eventi TextChanged vengono verificati nel turno successivo del dispatcher. Gli slot e il fallback mostrano la miniatura assegnata; la selezione confermata modifica la bozza, da salvare esplicitamente. Le immagini includono l’intera superficie per evitare ritagli di mirini personali grandi.

# Verifica della UI antracite/lime

Eseguire `CrosshairNative.Settings.exe --design-preview` dalla radice del progetto. Le immagini `out/design-preview/1.png`–`7.png` provengono dal rendering degli elementi WinUI dell’app: slot, libreria, editor, impostazioni, tema chiaro, finestra ridotta e dialogo di selezione. Non sono mockup e non richiedono cattura del desktop.

Il comando verifica anche selezione, ricerca senza risultati, ripristino del filtro e annullamento del dialogo. Navigazione e cambi di tema devono lasciare `dirty` ed `editorDirty` falsi. `--soak-test` ripete 120 viste/temi e stampa memoria, handle e numero di miniature in cache.

Verificare manualmente Tab/frecce/Invio/Esc, accesso al menu «···» dei tasti, lettore schermo, contrasto elevato di Windows e monitor con DPI diversi. Il rendering automatico non certifica queste interazioni.
