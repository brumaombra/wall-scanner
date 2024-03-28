
# Visualizzatore CSV Heatmap

Software per ESP32 capace di analizzare muri e superfici per individuare e mostrare la presenza di elementi metallici.

## Istruzioni per l'Installazione
Per configurare il Wall-Scanner puoi seguire questi passaggi:
1. Collega tutto l'hardware necessario all'ESP32.
2. Scarica il codice sorgente dal repository.
3. Verifica che il pinout dell'ESP sia corretto; se necessario, modifica i valori dei pin per adattarli alla tua configurazione.
4. Collega l'ESP al PC tramite USB.
5. Utilizza PlatformIO per scrivere la cartella `data` sulla memoria flash dell'ESP (`Build Filesystem Image`, poi `Upload Filesystem Image`).
6. Utilizza PlatformIO per caricare il codice sorgente sull'ESP.
7. Goditi il Wall-Scanner! ❤️

## Funzioni principali

`startTimer()`: Avvia un timer quando si rileva un evento di inizio (RISING edge) sul pin specificato. Inizializza il contatore `timerCounter` se il timer non è attivo.
`stopTimer()`: Ferma il timer su un evento di fine (FALLING edge). Calcola la differenza di tempo dall'avvio del timer e segnala che il timer non è più in esecuzione.
`attachAllInterrupts()`: Abilita gli interrupt sui pin specificati per iniziare la misurazione del tempo tramite gli eventi RISING e FALLING.
`detachAllInterrupts()`: Disabilita gli interrupt sui pin specificati, interrompendo la misurazione del tempo.
`LedPWM()`: Gestisce la modulazione di larghezza di impulso (PWM) per i LED basandosi sul valore di `delta` e una soglia prefissata (`soglia`), per indicazioni visive.
`stampaNormale(int X, int Y, float mag)`: Stampa su seriale le coordinate (X, Y) e il valore di magnitudo `mag` con precisione a una cifra decimale.
`writeCsv(int X, int Y, float mag)`: Compone una stringa in formato CSV con i valori di X, Y e la magnitudo `mag`, aggiungendola a `csvString`.
`LEDUpDown(float Ycm, int Yprec)`: Gestisce l'accensione dei LED in base alla posizione Y per indicare la direzione della scansione.
`setupLittleFS()`: Inizializza il filesystem LittleFS per la gestione di file e risorse della pagina web.
`setupServer()`: Configura e avvia un server web asincrono per servire pagine web e gestire le richieste HTTP relative alla scansione e alle impostazioni.
`setupPin()`: Configura i pin di input/output per LED, pulsanti e altri componenti del progetto.
`setupMouse()`: Inizializza un mouse PS2 per il rilevamento dei movimenti durante la scansione.
`setup()`: Funzione di inizializzazione eseguita all'avvio, per configurare seriale, filesystem, server web, pin e mouse.
`loop()`: Funzione principale che gestisce il flusso di esecuzione del programma, incluse le interazioni utente, la misurazione, il movimento e la generazione dei dati di scansione.

## Licenza

Questo progetto è distribuito con Licenza MIT.