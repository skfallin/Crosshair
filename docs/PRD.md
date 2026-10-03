PROMPT DI SVILUPPO + PRD — Crosshair Native
Versione della specifica: 1.0 · 29 settembre 2026.
Nome di lavoro: Crosshair Native. Il nome è provvisorio: centralizzalo e non presumere che sia disponibile come marchio.
1. Ruolo e risultato richiesto
Agisci come senior Windows engineer, product engineer e responsabile della qualità. Progetta e implementa un'applicazione Windows nativa, leggera e gratuita per mostrare mirini personalizzati sopra un gioco e cambiarli usando gli stessi tasti con cui l'utente seleziona gli slot delle armi.
Il primo scenario d'uso è Fortnite con mouse e tastiera. Realizza però un motore generico: Fortnite deve essere un profilo configurabile, non una dipendenza tecnica.
Questo documento è la specifica operativa. Devi produrre codice funzionante, build riproducibili, test e documentazione, non soltanto un mockup o un piano. Prima di modificare il repository, leggi le istruzioni presenti, compresi gli eventuali AGENTS.md, e controlla ambiente, toolchain e lavoro già esistente. Non sovrascrivere modifiche dell'utente e non creare commit o pubblicazioni remote senza richiesta.
Procedi per incrementi verificabili. Per decisioni non bloccanti adotta l'opzione più semplice compatibile con questo PRD e registrala. Non inventare API, SDK, risultati di test, certificazioni o prestazioni. Se l'ambiente non permette di eseguire Windows o Fortnite, completa ciò che puoi verificare e indica separatamente ciò che richiede una prova reale.
2. Obiettivo e vincoli inderogabili
L'esperienza desiderata è: installare, scegliere un mirino, associare i propri tasti, chiudere le impostazioni e giocare. Cambiando slot, il mirino cambia senza richiedere un secondo comando.
Piattaforma e tecnologia. Windows 11 x64 come target iniziale. Motore C++20, interfaccia nativa Windows. Sono esclusi Electron, Chromium incorporato, WebView2, Tauri, frontend HTML/CSS/JavaScript e servizi web necessari al funzionamento. Non aggiungere supporto macOS, Linux o console.
Gratuità e autonomia. Nessun account, abbonamento, pagamento, pubblicità, scadenza, verifica periodica della licenza o funzione premium. Tutte le funzioni operative devono funzionare offline. Non introdurre telemetria, cloud, marketplace o un backend. La distribuzione ufficiale del progetto deve restare gratuita.
Confine funzionale. L'app osserva i tasti configurati e disegna un elemento grafico esterno. Non riconosce l'arma equipaggiata, non legge lo stato interno del gioco e non modifica precisione, dispersione o rinculo. Un'etichetta come “Pompa” descrive la configurazione dell'utente, non una rilevazione.
Compatibilità, non occultamento. Nessuna promessa di “undetectable”, “zero ban” o approvazione ufficiale. Escludi letture/scritture della memoria del gioco, DLL injection, hook del rendering, driver, macro, input sintetici, bypass degli anti-cheat e occultamento dei processi. Non chiedere di disattivare protezioni di Windows o del gioco.
3. Perimetro delle versioni
Area	Versione 1.0 standard	Estensione successiva
Visualizzazione	Finestra e finestra senza bordi	Widget Game Bar opzionale
Cambio mirino	Tasti diretti degli slot	Nessun riconoscimento automatico pianificato
Input	Tastiera, pulsante centrale e laterali del mouse	Controller e scorrimento sequenziale da valutare separatamente
Catalogo	Almeno 300 preset originali o autorizzati	Ulteriori pacchetti locali
Personalizzazione	Editor statico e PNG trasparenti	Nessuna animazione nella roadmap iniziale
Profili	Fortnite e profili generici locali	Altri modelli di profilo, senza SDK di gioco
Distribuzione	Installazione per utente	Pacchetto Game Bar separato


Non rendere Game Bar necessaria per installare, compilare o usare la versione standard. Se l'estensione non supera le verifiche, la versione standard deve restare completa e utilizzabile. Non mostrare funzionalità incomplete come se fossero attive.
4. Architettura e toolchain
Adotta questa separazione, evitando framework aggiuntivi non necessari:
Modulo	Tecnologia scelta	Responsabilità
Core	C++20, libreria testabile	Profili, preset, binding, validazione, macchina a stati
Engine	C++20 e Win32	Input, finestra bersaglio, overlay, tray e persistenza
Rendering	Direct2D e Windows Imaging Component	Geometrie, rasterizzazione e PNG
Settings	C++/WinRT e WinUI 3	Configurazione guidata, libreria, editor e impostazioni
Game Bar	Componente UWP/XAML separato	Backend opzionale da validare con lo SDK ufficiale
Test	Core testabile e applicazione bersaglio sintetica	Verifiche automatiche senza dipendere dal gioco


WinUI 3 è un framework desktop nativo disponibile per C++: non richiede una UI web. Il modello documentato per i widget Game Bar è invece UWP/XAML; non trattare un normale eseguibile WinUI 3 come un widget registrabile automaticamente. [S1][S2]
Separazione dei processi. CrosshairNative.Engine.exe deve funzionare senza CrosshairNative.Settings.exe. Chiudendo le impostazioni, il processo UI deve terminare, non restare nascosto. La tray appartiene al motore e permette di riaprire le impostazioni o uscire completamente.
Build. Usa MSVC e una toolchain stabile verificata. Core, engine e test possono usare CMake; per WinUI e UWP usa i progetti MSBuild appropriati. Non forzare un sistema di build unico se introduce fragilità. Fornisci uno script PowerShell che coordini la build e controlli i prerequisiti.
Blocca le versioni delle dipendenze e degli SDK nel repository. Non usare versioni preview, wildcard “latest” o librerie scaricate a ogni avvio. Documenta installazione dei prerequisiti, comandi e versioni realmente provate. Non sostituire silenziosamente WinUI con un'interfaccia web se mancano i workload.
5. Configurazione al primo avvio
Realizza una procedura breve, riapribile dalle impostazioni e completabile anche senza Fortnite installato.
Passaggio A — Profilo e visualizzazione. Proponi “Fortnite” e “Generico”, spiegando che sono associazioni locali. Seleziona Standard come modalità predefinita. L'utente può associare una finestra aperta o configurare il programma bersaglio successivamente. Se non esiste una finestra idonea, mostra “In attesa del gioco”, non un errore.
Passaggio B — Tasti. Mostra cinque slot configurabili e un'azione facoltativa “Piccone / nascondi”. Per ogni slot, l'utente può scegliere un'etichetta, premere “Registra tasto”, usare il proprio tasto e selezionare un mirino. Non imporre numeri, nomi di armi o associazioni predefinite. Permetti di lasciare slot non configurati.
Passaggio C — Mirini. Proponi una selezione iniziale ridotta, con accesso immediato al catalogo completo. Prevedi anche un mirino predefinito per quando non è noto quale slot sia stato selezionato.
Passaggio D — Prova. Una superficie di test mostra il cambio dei mirini e il nome dell'associazione riconosciuta. Da questa prova l'utente deve poter tornare direttamente al tasto errato. La registrazione resta attiva solo durante la cattura esplicita di un binding.
Concludi spiegando in una frase: “Il mirino segue il tasto dello slot, non il contenuto dell'inventario”. Non simulare una calibrazione delle armi o la lettura dei comandi di Fortnite.
6. Input e associazioni
Windows Raw Input consente la ricezione di eventi anche in background attraverso RIDEV_INPUTSINK. Usalo come meccanismo di osservazione, non come strumento per rimappare i comandi del gioco. [S3]
Implementa un solo punto di registrazione e normalizzazione dell'input nel motore. Non usare RIDEV_NOLEGACY per sopprimere eventi, non generare SendInput e non usare hotkey globali riservate per catturare i singoli tasti degli slot.
Identificazione. Conserva scan code e flag estesi per la tastiera, insieme ai dati necessari a mostrare un nome leggibile nel layout corrente. Distingui, quando necessario, tastierino numerico e tasti principali. Supporta pulsante centrale, Mouse 4 e Mouse 5. Gli altri pulsanti HID non devono essere promessi senza una prova specifica.
Attivazione. Seleziona il preset sul fronte di pressione, non a ogni ripetizione automatica. Mantenere premuto un tasto non deve provocare ridisegni ripetuti. Il gioco deve continuare a ricevere tutti i suoi normali input.
Modificatori. Un'associazione semplice deve poter funzionare anche mentre l'utente tiene premuto Shift per correre. Modella esplicitamente modificatori richiesti e tolleranza di quelli aggiuntivi. Se più binding corrispondono allo stesso evento, prevale il più specifico; in caso di ulteriore ambiguità, impedisci il salvataggio e mostra il conflitto.
Conflitti. Gestisci duplicati, combinazioni riservate dal sistema e collisioni con pausa/nascondi. Non dichiarare di rilevare i binding interni del gioco. I controlli riguardano soltanto la configurazione dell'app.
Focus e privacy. I tasti degli slot hanno effetto solo quando è attiva la finestra bersaglio o la prova esplicita dell'onboarding. Gli eventi estranei vengono scartati: niente testo digitato, cronologie di tastiera, movimenti del mouse o log dei tasti. Il binding salvato nelle impostazioni non deve diventare un registro degli utilizzi.
Limiti dichiarati. Se l'utente scrive in chat all'interno del gioco, la finestra resta attiva: non possiamo sapere che il tasto non ha cambiato arma. Non introdurre false rilevazioni di menu, chat, costruzione o inventario. La prima versione non deduce lo slot tramite rotella, “arma successiva” o controller.
7. Stato del mirino e comandi rapidi
Implementa una macchina a stati testabile, separando almeno profileEnabled, targetEligible, manualPaused, selectedSlot, slotRequestsHidden, defaultPresetId, backendState e previewMode.
La regola di priorità è: profilo disabilitato o uscita → nascosto; bersaglio non idoneo → nascosto; pausa manuale → nascosto e binding degli slot ignorati; slot “nascondi” → nascosto ma binding successivi attivi; altrimenti → preset dello slot o preset predefinito.
Selezione diretta. Un binding seleziona sempre il suo slot; non alterna tra due slot. Ripremere il tasto già attivo è un'operazione idempotente.
Nascondi per piccone/utilità. Questa azione non mette in pausa l'app. Il successivo tasto di un'arma ripristina il relativo preset.
Pausa manuale. Prevedi un comando configurabile e una voce nella tray. La ripresa parte dal preset predefinito, perché durante la pausa l'utente potrebbe aver cambiato arma. Un nuovo tasto di slot riallinea immediatamente la selezione.
Perdita di focus. Nascondi l'overlay, azzera i tasti mantenuti e invalida la selezione dedotta. Al ritorno al gioco mostra il preset predefinito fino al prossimo binding. Non riattivare un tasto già tenuto premuto: attendi una nuova pressione.
Errori. Un preset mancante usa il fallback e produce una segnalazione non invasiva nelle impostazioni. Non aprire finestre sopra il gioco. Le transizioni devono essere deterministiche e coperte da test.
8. Overlay standard e finestra bersaglio
Le finestre layered di Win32 permettono trasparenza; Microsoft documenta anche il passaggio degli eventi del mouse alle finestre sottostanti. Per il primo backend valuta una superficie piccola aggiornata con UpdateLayeredWindow, invece di una superficie grande quanto lo schermo. [S4][S5]
Realizza una finestra senza bordi, non attivabile, assente dalla taskbar e da Alt+Tab. Deve essere click-through anche sui pixel visibili del mirino. Non basta verificare i clic sulla sola area trasparente. Nessun comando per mostrare, spostare o aggiornare il mirino deve rubare il focus.
Rasterizza i preset in superfici BGRA con alpha premoltiplicato e valida esplicitamente la resa. Non assumere che applicare uno stile layered a un render target qualsiasi risolva automaticamente trasparenza e composizione. Riutilizza le risorse e ridisegna soltanto in caso di cambiamento.
Posizionamento. Il centro predefinito è il centro dell'area client del gioco, convertito in coordinate dello schermo. Non includere barra del titolo e bordi. Quando la finestra si sposta o si ridimensiona, il mirino la segue.
Supporta offset X/Y in pixel fisici, ripristino del centro, monitor multipli e coordinate negative. Non tentare di riconoscere viewport, bande nere o HUD: usa un offset manuale quando necessario.
DPI. Adotta Per-Monitor DPI Awareness V2 e separa i DIP dell'interfaccia dai pixel fisici del mirino. Le indicazioni Microsoft richiedono di gestire esplicitamente cambi di DPI e layout. [S6]
La dimensione del mirino va espressa in pixel fisici, senza ingrandimenti inattesi spostando il gioco tra monitor. Definisci allineamento al pixel, centro geometrico e trattamento degli spessori pari/dispari. Verifica lo scarto con immagini di riferimento, non soltanto a occhio.
Bersaglio. Usa metadati delle finestre e del processo, non dati di gioco. Se serve identificare il percorso dell'eseguibile, limita i privilegi alla sola interrogazione dei metadati; in caso di accesso negato, non elevare automaticamente. Non salvare HWND o PID come identità persistente.
Preferisci eventi WinEvent fuori processo per focus e geometria, con callback brevi. Non usare un ciclo di polling ad alta frequenza. Gestisci distruzione e ricreazione della finestra, minimizzazione, sessione bloccata, sospensione e scollegamento di un monitor.
La versione standard deve dichiarare supporto soltanto alle configurazioni in finestra/borderless effettivamente testate. Non promettere fullscreen esclusivo tramite una finestra topmost e non inventare un rilevamento infallibile della modalità grafica del gioco.
9. Backend Game Bar opzionale
Realizza prima una prova di fattibilità separata. Game Bar documenta pinning e click-through dei widget: sono condizioni da verificare e guidare nell'interfaccia, non preferenze dell'utente da presumere attivate. [S7]
La prova deve dimostrare registrazione del widget, visualizzazione trasparente, fissaggio, passaggio dei clic, cambio preset, centratura, ricostruzione dello stato e funzionamento quando la finestra delle impostazioni è chiusa.
Verifica prima anche il canale di comunicazione con il motore. La documentazione Microsoft descrive restrizioni diverse per la comunicazione UWP/Win32 a seconda di identità e packaging: non assumere che una normale named pipe desktop sia utilizzabile senza adattamenti dal widget. [S8]
Il widget non deve leggere direttamente cartelle private desktop dando per scontato l'accesso. Riceve soltanto lo stato e gli asset necessari attraverso il canale validato.
La configurazione deve spiegare come aprire Game Bar, attivare il widget, fissarlo e abilitare il passaggio dei clic. Non promettere fissaggio, spostamento o modifica delle preferenze di Game Bar automatici se lo SDK non li consente.
Desktop e Game Bar non devono disegnare contemporaneamente due mirini. Gestisci selezione del backend, conferma di attivazione, perdita di connessione e ripristino del preset corrente. Se il motore termina o il canale si perde definitivamente, il widget deve nascondere il mirino, non lasciarlo congelato.
Se Game Bar manca, è disabilitata o non supera i test, proponi la modalità standard per finestra/borderless. Non aggirare il problema con injection. Registra in una matrice le combinazioni di Windows, Game Bar, gioco e modalità realmente verificate.
10. Catalogo locale ampio
Per la versione 1.0 completa richiedo almeno 300 preset utilizzabili, con ID stabili, metadati e miniature. Durante lo sviluppo è ammesso un catalogo più piccolo, ma deve essere etichettato come incompleto.
Organizza il catalogo in famiglie: punti, croci, cerchi, anelli, forme a T, chevron, mirini aperti e combinazioni. Aggiungi tag descrittivi come “minimal”, “centro libero”, “contorno” o “grande”. I tag “pompa” e “assalto” sono suggerimenti di scelta, non promesse di vantaggi o precisione.
Varietà reale. Non contare 300 colori dello stesso disegno come 300 mirini. Usa geometrie e proporzioni sostanzialmente diverse. Un generatore deterministico può produrre il catalogo al momento della build, ma non deve generare migliaia di varianti a ogni avvio. Produci una tavola di confronto per la revisione e un controllo dei duplicati che ignori nome e colore.
Provenienza. Usa geometrie originali, asset forniti dall'utente oppure materiali con autorizzazioni sufficienti. I termini di CenterPoint richiedono autorizzazione per la copia del servizio e includono scraping/crawling tra gli usi vietati; non includere uno scraper di Crosshair X come requisito del prodotto. [S9]
Per contenuti esterni conserva autore, fonte, licenza e relativa prova. Non inventare queste informazioni. Un eventuale pacchetto Crosshair X autorizzato può essere importato in seguito attraverso lo stesso formato locale, senza creare una dipendenza dal suo sito, da Steam o dalla sua applicazione.
Consultazione. Implementa ricerca locale, filtri, preferiti, recenti e anteprima su fondo chiaro/scuro. Carica le miniature in modo progressivo e virtualizza la griglia. Il motore residente carica soltanto preset assegnati, fallback e asset necessari, non tutto il catalogo grafico.
11. Editor dei mirini
L'editor deve usare lo stesso modello grafico del renderer finale, evitando differenze tra anteprima e gioco.
Prevedi primitive geometriche, massimo 16 livelli, dimensione, spessore, apertura centrale, colore RGBA, opacità, contorno, rotazione e offset. Aggiungi undo/redo, ripristino, duplicazione e salvataggio come nuovo preset.
Mantieni separati i preset inclusi nell'app e le copie personalizzate: modificare un preset di serie crea una copia con un nuovo ID. Un aggiornamento del catalogo non deve sovrascrivere i mirini dell'utente.
Supporta anche un livello PNG trasparente. Non convertirlo falsamente in geometrie modificabili e non tentare di recuperare parametri da un'anteprima raster. Separa eventuali filtri nearest-neighbor e interpolati e mostra la resa effettiva.
L'anteprima vive nella finestra di configurazione. Un'eventuale prova sovrapposta al desktop deve essere esplicita, temporanea e chiaramente distinta dalla modalità gioco. Sono esclusi animazioni, GIF, video, effetti reattivi ai colpi, lente d'ingrandimento e lettura dello schermo.
12. Interfaccia e accessibilità
La finestra principale contiene quattro aree: Libreria, Slot e tasti, Editor, Impostazioni. Un selettore di profilo e uno stato sintetico rimangono sempre accessibili.
Usa controlli WinUI nativi, tema chiaro/scuro/sistema, contrasto leggibile, spaziatura regolare e accento Windows. Evita stile “gaming RGB”, pannelli ornamentali, grafici decorativi, feed e onboarding pubblicitari. Lingua iniziale italiana; stringhe in risorse localizzabili, non sparse nel codice.
Mostra stati comprensibili: “Attivo”, “In pausa”, “In attesa del gioco”, “Preset predefinito” e “Game Bar non disponibile”. Non confondere “motore avviato” con “mirino visibile”. Gli errori devono indicare una correzione concreta e non soltanto un codice numerico.
Ogni controllo deve essere raggiungibile da tastiera e avere un'etichetta accessibile. Non comunicare stato o errore solo con un colore. Rispetta contrasto elevato e riduzione delle animazioni. Se ci sono modifiche non salvate, gestisci la chiusura senza perderle.
La tray offre apri impostazioni, pausa/riprendi, selezione profilo e uscita completa. Non mostrare notifiche a ogni cambio arma. L'avvio con Windows è disabilitato fino a scelta esplicita dell'utente.
13. Modello dei dati e persistenza
Usa file JSON versionati per impostazioni, profili e preset. Evita un database nella prima versione, salvo una necessità dimostrata. Il motore è l'unico autore dei file persistenti; la UI invia comandi, non scrive contemporaneamente sugli stessi file.
Entità	Informazioni obbligatorie
Preset	Versione schema, ID, nome, tag, geometria/livelli o asset, provenienza
Profilo	ID, nome, associazione al bersaglio, backend, slot, fallback e offset
Binding	Tipo dispositivo, identificatore tasto, modificatori, azione e destinazione
Impostazioni	Lingua, tema, preferiti, profilo attivo, avvio automatico
Stato runtime	Selezione dedotta, focus, pausa, stato backend; non persistere ogni pressione


Memorizza i dati utente sotto %LOCALAPPDATA%\CrosshairNative\, con cartelle separate per profili, preset, asset, cache e diagnostica. Gli asset inclusi nell'installazione restano separati e in sola lettura. Documenta l'adattamento dei percorsi necessario per i pacchetti con identità Windows.
Usa scrittura su file temporaneo e sostituzione atomica sullo stesso volume, backup dell'ultima configurazione valida e migrazioni testate. Un file corrotto non deve causare la perdita di tutti i profili. Mantieni il file problematico per il recupero ed evita reset silenziosi.
Definisci schemi e limiti espliciti. Gli ID devono essere stabili, i riferimenti verificabili e i cicli vietati. I recenti possono essere salvati alla selezione esplicita nella libreria, non a ogni cambio slot in partita.
14. Importazione ed esportazione
Supporta PNG trasparenti, preset nel formato JSON del progetto e pacchetti locali .crosshairpack con manifest, preset e asset. Esporta un profilo insieme ai mirini e alle immagini necessarie, per trasferirlo senza riferimenti rotti.
L'importazione è sempre esplicita e non scarica risorse remote. Presenta un riepilogo e gestisce ID già presenti senza sovrascrivere silenziosamente i dati. I percorsi dell'eseguibile e i monitor del PC di origine devono essere riconfigurabili sul PC di destinazione.
Applica limiti iniziali: PNG fino a 2048×2048 pixel e 16 MiB compressi; massimo 16 livelli per preset; pacchetti fino a 100 MiB espansi e 2.000 elementi. Questi sono limiti di progetto, modificabili soltanto con test e motivazione.
Valida dimensioni decodificate, allocazioni, profondità JSON e valori numerici finiti. Impedisci path traversal, percorsi assoluti, symlink/reparse point, nomi Windows riservati, decompressione illimitata e scrittura fuori dalla cartella di destinazione. Non caricare plugin, eseguibili o script contenuti nei pacchetti.
Non supportare SVG arbitrari nella prima versione. Non accettare riferimenti a font, immagini o URL esterni dentro un preset. La libreria distribuita richiede provenienza verificata; un'importazione personale con licenza non specificata va etichettata come tale, non dichiarata automaticamente redistribuibile.
15. Comunicazione e ciclo di vita
Per UI desktop e motore usa IPC locale con protocollo versionato, per esempio named pipe limitate all'utente e alla sessione correnti. Rifiuta connessioni remote, messaggi malformati e payload fuori limite. Non usare un server HTTP o una porta di rete locale.
Definisci comandi tipizzati come GetState, SaveProfile, ApplyPreset, SetPaused, StartPreview, StopPreview e Shutdown. Prevedi ID della richiesta, revisione dello stato, esito e codice di errore. Non implementare comandi per eseguire processi arbitrari, leggere file arbitrari o invocare una shell.
Il motore aggiorna lo stato solo dopo validazione completa. La UI deve ricevere conferma o errore, senza fingere che un cambio sia stato applicato. Dopo una riconnessione scambia uno snapshot completo; evita di dipendere soltanto da eventi incrementali persi.
Una sola istanza del motore per utente/sessione. Una seconda apertura porta alle impostazioni esistenti senza duplicare overlay o registrazioni input. La chiusura della UI non termina il motore; “Esci” termina motore, overlay e l'attività del widget eventualmente collegato.
La gestione del crash non deve introdurre servizi di sistema o processi watchdog permanenti. Se il widget usa un segnale di presenza, deve essere a bassa frequenza e conteggiato nelle misure di consumo.
16. Prestazioni: obiettivi e misurazioni
Questi sono obiettivi da verificare, non risultati da dichiarare prima dei test.
Metrica	Obiettivo iniziale
Motore standard a regime	Non oltre 50 MiB di private bytes nella configurazione di riferimento
CPU, mirino statico senza input	Media inferiore allo 0,2% dell'intero sistema su 60 secondi
Cambio preset	Nessun accesso al disco o caricamento di miniature nel percorso critico
Evento ricevuto → aggiornamento consegnato al backend standard	p95 entro 5 ms nella prova controllata
Ridisegno statico	Nessun render loop continuo imposto dall'app
Memoria dopo ripetute operazioni	Nessuna crescita progressiva non spiegata dalle cache limitate
Rete durante uso normale	Nessuna richiesta dell'app


Misura separatamente motore, UI aperta, UI chiusa e modalità Game Bar. Non nascondere nei risultati il costo di processi o componenti aggiuntivi attivati.
Il tempo fino alla consegna al backend non coincide con il tempo fino al pixel visibile: riporta chiaramente questa distinzione. Non dichiarare latenza end-to-end senza uno strumento adeguato.
Usa una macchina di riferimento documentata, build Release e scenario riproducibile. Prova anche mouse ad alta frequenza: scartare i movimenti non necessari deve costare poco e non saturare la coda degli input.
Per l'impatto sul gioco confronta più sessioni equivalenti con app accesa/spenta, riportando tempi dei fotogrammi, percentili, stutter e variabilità della baseline. Non usare soltanto la media FPS. Scostamenti dai budget vanno segnalati con misure e interventi, non nascosti spostando la soglia.
17. Sicurezza, diagnostica e distribuzione
Esegui normalmente come utente standard. Nessun driver, servizio Windows permanente, aggiornamento silenzioso o richiesta di privilegi amministrativi per disegnare un mirino.
Prevedi diagnostica locale disattivabile e con rotazione, senza testo digitato, coordinate del puntatore, titoli di finestre, identificativi hardware o percorsi personali non necessari. Un report condivisibile deve essere generato solo su richiesta e consentire la revisione del contenuto. Non acquisire schermate del gioco.
Prepara un'installazione per utente, disinstallazione pulita e rimozione dell'avvio automatico. Non includere il widget Game Bar come dipendenza obbligatoria. Documenta dimensione, dipendenze runtime e requisiti del sistema realmente supportato.
Distingui build di sviluppo non firmata e pacchetto distribuibile. Non falsificare firme o garantire assenza di avvisi di reputazione. Se un pacchetto richiede firma o procedure di registrazione, documentale; non chiedere di disattivare Defender o SmartScreen come soluzione standard.
Prepara inventario delle dipendenze e avvisi delle licenze. La licenza open source del codice originale e i dati del titolare devono essere espliciti prima della pubblicazione; non inventarli. Documenta separatamente le licenze degli asset. Non presentare la licenza come garanzia che eventuali fork di terzi saranno gratuiti.
Prima di una distribuzione pubblica, separa verifica tecnica dalle autorizzazioni del gioco o di un torneo. Una prova riuscita dimostra il funzionamento in quella configurazione, non un'approvazione permanente.
18. Strategia di test e criteri di accettazione
Crea un bersaglio Win32 di test che possa spostarsi, ridimensionarsi, perdere focus e contare i clic ricevuti. Le prove di input devono invocare il reducer o usare input manuale nel programma di test, non inviare comandi sintetici a Fortnite.
ID	Prova	Risultato richiesto
AC-01	Configurazione senza gioco installato	Profilo salvato e stato “In attesa del gioco”
AC-02	Pressione alternata di tre slot	Ogni tasto seleziona il proprio preset
AC-03	Tasto mantenuto e ripetizione	Nessuna alternanza o ridisegno ridondante
AC-04	Shift premuto durante il cambio slot	Comportamento coerente con la politica modificatori
AC-05	Clic sul pixel centrale visibile	Il bersaglio riceve il clic; il mirino non prende focus
AC-06	Spostamento, resize e monitor con DPI diversi	Centro e dimensioni rispettano la specifica
AC-07	Alt+Tab, minimizzazione e blocco sessione	Overlay nascosto; nessuna attivazione fuori bersaglio
AC-08	Pausa, nascondi e ripresa	Stati distinti e fallback corretti
AC-09	Chiusura delle impostazioni	UI terminata, motore ancora operativo
AC-10	Seconda apertura dell'app	Nessun motore o overlay duplicato
AC-11	Catalogo 1.0	Almeno 300 preset validi, senza duplicati cosmetici conteggiati
AC-12	Salvataggio e aggiornamento catalogo	Personalizzazioni e preferiti conservati
AC-13	Configurazione corrotta	Recupero controllato senza cancellare tutti i dati
AC-14	Pacchetto ostile o immagine fuori limite	Importazione rifiutata senza crash né scritture esterne
AC-15	Esportazione e reimportazione	Geometria, asset e associazioni ricostruiti
AC-16	Rete assente	Tutto il flusso standard rimane disponibile
AC-17	Uscita e disinstallazione	Nessun overlay, processo o avvio automatico residuo
AC-18	Game Bar, quando inclusa	Pinning, click-through, IPC e centratura realmente verificati


Aggiungi unit test per matching dei binding, priorità, macchina a stati, schema, migrazioni e deduplicazione. Aggiungi test di immagini per il renderer con tolleranza documentata, test IPC e sessioni prolungate per handle, memoria e stabilità.
La CI Windows deve compilare almeno core, motore, UI e test della versione standard. Le verifiche che richiedono desktop interattivo o hardware reale vanno in una checklist separata. Una CI verde non equivale a un test riuscito su Fortnite.
19. Piano di implementazione
Fase	Consegna verificabile	Condizione per proseguire
0 — Fondazioni	Repository, toolchain, core testabile, schema iniziale e decisioni architetturali	Build documentata; nessuna dipendenza web
1 — Percorso completo minimo	Overlay, tre preset, input, bersaglio e tray	Cambio reale, centratura e click-through nel test harness
2 — Configurazione	UI nativa separata, onboarding, profili e persistenza	Uso senza modificare manualmente JSON
3 — Prodotto standard	Catalogo 300+, editor, import/export e accessibilità	Criteri standard superati e limiti documentati
4 — Qualità e pacchetto	Benchmark, prove prolungate, installazione e documentazione	Versione standard distribuibile dopo i controlli di release
5 — Game Bar	Widget e packaging opzionali	Prova tecnica, IPC e test reali superati


Puoi anticipare una piccola prova Game Bar per ridurre il rischio, ma non lasciare incompleta la versione standard per inseguire l'estensione.
Alla fine di ogni fase riporta cosa funziona, quali comandi hai eseguito, quali prove sono passate, quali sono bloccate e cosa rimane. Non definire “completo” un semplice scaffold o una UI collegata a dati fittizi.
20. Struttura e consegne del repository
Struttura indicativa, adattabile senza compromettere la separazione:
/apps/engine
/apps/settings
/apps/gamebar
/core
/rendering
/catalog/presets
/catalog/licenses
/schemas
/tests/unit
/tests/integration
/tests/rendering
/tools/catalog-builder
/tools/test-target
/scripts
/packaging
/docs
Fornisci README.md, questo PRD, ARCHITECTURE.md, un registro delle decisioni, BUILD.md, TESTING.md, PERFORMANCE.md, COMPATIBILITY.md, PRIVACY.md, CATALOG_PROVENANCE.md e una checklist di release.
La documentazione deve contenere comandi reali, percorsi degli output, prerequisiti, stato delle prove e limitazioni conosciute. Genera un report di validazione del catalogo e un riepilogo dei test. Non includere eseguibili fittizi, API key, certificati privati o asset senza provenienza.
21. Istruzione finale all'agente
Inizia ispezionando repository e toolchain. Presenta un piano breve riferito alle fasi, poi implementa il primo percorso funzionante: bersaglio selezionabile, mirino trasparente click-through, tre preset e cambio con tasti configurabili.
Continua con la procedura guidata e la UI nativa separata, poi completa catalogo, editor, persistenza e test. Non fermarti alla sola descrizione del progetto se puoi implementarlo. Non costruire tutta la galleria prima di verificare che overlay, input e focus funzionino.
Mantieni il codice leggibile, con gestione degli errori, ownership esplicita e test. Evita architetture speculative, dipendenze pesanti e funzionalità non richieste. Non modificare globalmente il sistema o scaricare/installare componenti senza dichiarare la necessità e rispettare le autorizzazioni dell'ambiente.
La definizione di completamento della versione standard è un'app reale, configurabile e offline, con catalogo ampio, cambio mirino per tasto, UI chiudibile senza interrompere l'overlay, build riproducibile e risultati verificabili. Tutto ciò che non è stato eseguito o provato deve risultare chiaramente come non verificato.
Riferimenti verificati per la specifica
I riferimenti documentano capacità e vincoli delle piattaforme; i budget, le scelte di prodotto e i criteri di accettazione sono requisiti di questo progetto, non garanzie dei fornitori.
[S1] Microsoft — WinUI 3: https://learn.microsoft.com/en-us/windows/apps/winui/winui3/
[S2] Microsoft — Game Bar Widget Overview: https://learn.microsoft.com/en-us/xbox/game-bar/overview
[S3] Microsoft — Raw Input Overview: https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input
[S4] Microsoft — Window Features: https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features
[S5] Microsoft — UpdateLayeredWindow: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow
[S6] Microsoft — High DPI Desktop Application Development: https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
[S7] Microsoft — Supporting click-through: https://learn.microsoft.com/en-us/xbox/game-bar/guide/click-through
[S8] Microsoft — Interprocess communication: https://learn.microsoft.com/en-us/windows/uwp/communication/interprocess-communication
[S9] CenterPoint — Terms of Service, sezioni 2 e 12: https://centerpointgaming.com/terms-of-service.html