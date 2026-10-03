# Provenienza del catalogo

Catalogo di **8 tipi originali personalizzabili**, definiti in core/model.cpp: punto, croce, cerchio, forma a T, chevron, angoli, cerchio con croce e costellazione. Dimensioni, spessore, contorno, colore e punto centrale si modificano nell’editor con anteprima prima del salvataggio. Le copie salvate appaiono nella famiglia personali. I 323 vecchi preset rimangono disponibili per i profili esistenti, ma sono esclusi dall’indice e dalla scelta dei mirini.

Non sono stati scaricati, copiati o estratti mirini da Crosshair X, CenterPoint, Steam o gallerie di terzi. Non sono presenti scraper. I PNG delle miniature derivano dal renderer del progetto, non da anteprime di altri prodotti.

Ogni preset ha ID stabile, nome, famiglia, tag, geometria e oggetto provenance con origine/fonte, autore/licenza/prova dell’autorizzazione quando noti. Il catalogo dichiara origine nel codice del progetto; i campi storici vuoti vanno letti insieme a LICENSE e NOTICE del repository. Le copie personali mantengono metadati modificabili; importare un PNG azzera le attribuzioni non applicabili e segnala origine personale non documentata. Le dichiarazioni nei file importati non sono verificate automaticamente.

La validazione confronta fingerprint geometrici ignorando nome, colore e ordine dei livelli. Verifica limiti, round trip JSON e alpha premoltiplicato. Output in out/build/catalog: presets/, thumbnails/, index.json, VALIDATION.md e comparison.png. Tavola 2048×128, ordine da sinistra a destra e dall’alto in basso come index.json.

Il codice originale e il catalogo generato sono soggetti a PolyForm Noncommercial 1.0.0, con attribuzione skfallin: vedere LICENSE e NOTICE alla radice. I campi dei preset storici possono restare vuoti; non concedono diritti su eventuali PNG esterni importati dall’utente.
