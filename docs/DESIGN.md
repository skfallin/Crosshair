# Interfaccia — settembre 2026

Direzione richiesta: il riferimento visivo fornito dall’utente, con pannelli antracite su nero, caratteri bianchi marcati, lime e acquamarina. Il linguaggio viene applicato a un’app Windows nativa, con contenuti e azioni reali. Nessun marchio, avatar o grafico del riferimento è stato copiato.

## Gerarchia

- Barra laterale stabile con identità Crosshair Native, cinque destinazioni e stato reale del motore.
- Profilo attivo e gestione profili in alto; titolo e descrizione della vista sotto.
- Slot in schede: numero, nome modificabile, anteprima cliccabile e tasto. Il menu «···» contiene la rimozione del tasto. Il mirino predefinito ha una scheda lime.
- Libreria virtualizzata con ricerca, famiglie, preferiti e recenti; anteprima e azioni nel pannello laterale. Importazione ed esportazione sono raccolte in un espansore.
- Editor con parametri e anteprima affiancati; impostazioni divise in profilo di gioco e preferenze dell’app.
- Salvataggio, ricaricamento e pausa restano disponibili nella barra inferiore.

## Palette e comportamento

Risorse centralizzate in `apps/settings/design.hpp`: fondo `#0C0E0F`, superficie `#191C1E`, lime `#C7F928`, acquamarina `#88E9DF`. Segoe UI Variable usa la tipografia di Windows senza download. Raggi di 8–12 unità e spazi di 8/16/24/32 unità.

Le risorse di tema definiscono anche la variante chiara; la variante ad alto contrasto rinvia ai colori di sistema. Pulsanti, campi, menu e dialoghi mantengono i template nativi, gli stati di interazione e il focus. I messaggi di stato espongono una live region. La selezione del mirino supporta Tab, frecce, ricerca, Invio per confermare ed Esc per annullare; il profilo si salva solo con «Salva profilo».

Le griglie riducono le colonne quando manca spazio. I pannelli laterali vanno sotto il contenuto nelle finestre strette. La barra inferiore rimane fuori dall’area scorrevole. Dimensione minima della finestra: 760 × 600 pixel.

Le miniature ingrandiscono le forme piccole usando un ritaglio centrato che include tutti i pixel visibili e un margine. Le forme grandi o decentrate non vengono tagliate. Le anteprime di dettaglio e dell’editor conservano l’intera superficie del renderer. Nessuna modifica al rendering dell’overlay o ai dati dei preset.

## Verifica riproducibile

`CrosshairNative.Settings.exe --design-preview` costruisce slot, libreria, editor e impostazioni in scuro, poi slot in chiaro e in finestra ridotta. Esporta gli elementi WinUI reali con RenderTargetBitmap in `out/design-preview/1.png`–`7.png`, compreso il dialogo. Non cattura il desktop o il gioco e non salva modifiche al profilo.

Lo stesso comando esegue i controlli del selettore: selezione, ricerca senza risultati, ripristino, annullamento e assenza di modifiche indesiderate durante la navigazione. `--soak-test` ripete 120 viste e temi. Il controllo visuale delle immagini non sostituisce la verifica manuale con lettore schermo, alto contrasto di sistema e monitor con DPI differenti.
