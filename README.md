
# Visualizzatore CSV Heatmap

Questo programma JavaScript permette di caricare file CSV e visualizzarli come una heatmap in una tabella HTML. Utilizza diversi parametri per gestire l'aspetto della tabella e trasforma i dati del CSV in una heatmap colorata.

## Funzionalità

- Caricamento di file CSV.
- Parsing e pulizia dei dati CSV.
- Calcolo dei valori massimi e minimi per normalizzare i dati.
- Creazione di una tabella HTML per visualizzare i dati.
- Normalizzazione e colorazione dei dati in base al loro valore.
- Gestione delle celle vuote nella tabella, riempiendole in base ai valori vicini.

## Utilizzo

1. Carica un file CSV utilizzando il pulsante di caricamento.
2. Il file viene letto e i dati vengono visualizzati come una heatmap nella tabella.
3. I colori nella heatmap rappresentano i valori dei dati, con una scala da rosso a blu.

## Parametri di Configurazione

- `tableCellWidth`: Larghezza delle celle della tabella.
- `tableCellHeight`: Altezza delle celle della tabella.
- `tableCellBorder`: Stile del bordo delle celle della tabella.

## Funzioni Principali

- `loadCSV(event)`: Carica e legge il file CSV.
- `parseCSV(text)`: Effettua il parsing dei dati CSV.
- `createTable(width, height, dataMap)`: Crea la tabella heatmap.
- `normalizzaValore(x, minValue, maxValue)`: Normalizza i valori tra 0 e 100.
- `getColoreHeatMap(valore)`: Genera un colore per la heatmap.
- `trovaMassimo(arr)`, `trovaMinimo(arr)`: Trovano il massimo e il minimo in un array.
- `fillBlanks(table)`: Riempie le celle vuote della tabella.
- `getColorAverage(x, y, rows)`: Calcola il colore medio per le celle vuote.

## Esempio di File CSV

Il programma si aspetta un file CSV con almeno tre colonne: due per le coordinate (x, y) e una per il valore da rappresentare nella heatmap. Esempio di formato:

```
x,y,valore
0,0,10
1,0,20
0,1,30
...
```

Nota: L'header del CSV viene ignorato durante il parsing.

## Requisiti

- Un browser moderno che supporta JavaScript ES6.
- File CSV compatibile con il formato richiesto.

## Sviluppo e Contributi

Questo è un progetto open source. Sei libero di contribuire e migliorarlo. Per suggerimenti, modifiche o problemi, si prega di aprire una issue o una pull request nel repository.

## Licenza

Questo progetto è distribuito sotto la Licenza MIT.
