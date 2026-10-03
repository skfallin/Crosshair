# Prestazioni

Build Release su Windows 11 x64 build 26200, 16 processori logici. Misure del 29 settembre 2026. Il budget di 50 MiB si riferisce al motore, non alla UI WinUI.

## Misure eseguite

Comando: out/build/Release/engine_tests.exe --benchmark. Istanza isolata, profilo senza bersaglio, UI chiusa, warm-up 2 s, campione 60,0065 s.

| Metrica | Risultato |
|---|---|
| Private bytes iniziali | 6.434.816 (6,14 MiB) |
| Private bytes finali | 6.062.080 (5,78 MiB) |
| Handle iniziali/finali | 215 / 214 |
| CPU intero sistema | 0% rilevato alla risoluzione di GetProcessTimes |
| Overlay visibile | No: in attesa del bersaglio |

Zero nel contatore non dimostra costo fisicamente nullo. Questo scenario non sostituisce il test con mirino visibile e non certifica il budget di CPU sul gioco. Dati grezzi in out/engine-benchmark.txt.

UI: --soak-test attraversa 120 schermate/temi (20 cicli di sei viste; intervallo minimo 800 ms, può durare di più sotto carico), senza modificare il profilo reale. Risultati:

| Passi | Private bytes | Handle | Cache miniature |
|---|---:|---:|---:|
| 12 | 163.057.664 | 1.882 | 48 |
| 60 | 167.198.720 | 1.879 | 48 |
| 120 | 168.325.120 | 1.875 | 48 |

Nessun crash; incremento privato totale circa 5,0 MiB, di cui circa 1,1 MiB nella seconda metà. Questi campioni brevi non dimostrano assenza di leak a lungo termine. Output in out/ui-soak-final.txt. La UI è un processo separato e chiudibile; nessun altro processo browser viene avviato dall’app.

## Da misurare prima della release

- 60 s con overlay statico visibile, UI aperta/chiusa separatamente.
- p50/p95/p99 dell’evento ricevuto fino a UpdateLayeredWindow, almeno 1000 cambi: obiettivo p95 ≤5 ms, **non ancora misurato**. Non è la latenza fino al pixel.
- Soak di almeno 30 minuti con modifiche, salvataggi, import/export e finestre ricreate; confrontare memoria/handle dopo warm-up.
- Mouse ad alta frequenza, multi-monitor/multi-DPI e sospensione.
- Più sessioni Fortnite equivalenti acceso/spento, frame time, percentili, stutter e dispersione. **Non eseguito**.

Il motore non ha timer di render/polling continuo. Scarta movimento mouse e non usa disco nel cambio slot; PNG/rasterizzazione/cache si preparano fuori dal percorso critico. Queste proprietà del codice non sostituiscono le misure mancanti.

Primo pacchetto verificato: 910 file, 58.911.249 byte di payload (~56,18 MiB), MSI 21.643.264 byte (~20,64 MiB). Le dimensioni e gli hash esatti della build confezionata sono in package.json; piccole variazioni seguono modifiche a codice e avvisi.


