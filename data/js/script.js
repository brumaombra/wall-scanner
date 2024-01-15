// Parametri globali
const tableCellWidth = "30px";
const tableCellHeight = "30px";
const tableCellBorder = "1px solid #34495e";
const ESP32IP = "192.168.4.1"; // "http://localhost:3000"

// Documento pronto
$(document).ready(() => {
    init(); // Funzione init
});

// Funzione init
const init = () => {
    getConfuguration(); // Prendo la configurazione iniziale
};

// Prendo la configurazione iniziale
const getConfuguration = () => {
    $.ajax({
        url: `${ESP32IP}/settings`,
        type: "GET",
        success: response => {
            $("#settingsResolution").val(response.resolution || "3"); // Imposto risoluzione scansione
        }, error: error => {
            console.error(error);
        }
    });
};

// Salvo le impostazioni
const handleSaveSettingsPress = () => {
    const settings = { // Oggetto impostazioni
        resolution: $("#settingsResolution").val()
    };

    // Chiamata
    $.ajax({
        url: `${ESP32IP}/settings`,
        type: "POST",
        contentType: "application/json",
        data: JSON.stringify(settings),
        success: response => {
            $("#settingsSuccessToast").toast({
                delay: 2000
            }).toast("show");
        }, error: error => {
            console.error(error);
        }
    });
};

/*********************************** Creazione della heatmap ***********************************/

// Click del pulsante
const handleNewReadPress = () => {
    pollingRead(); // Inizio polling
    $("#scansionePlaceholder").addClass("hidden"); // Nascondo container
    $("#scansioneContainer").removeClass("hidden"); // Visualizzo container
    $("#newReadButton").prop("disabled", true); // Disabilito il pulsante
    $("#recordingLogo").removeClass("hidden"); // Visualizzo logo recording
    $("#successReadLogo").addClass("hidden"); // Nascondo messaggio success
    $("#newReadModal").modal("hide"); // Chiudo il modal
};

// Start della lettura (polling)
const pollingRead = () => {
    let polling = setInterval(() => {
        $.ajax({
            url: `${ESP32IP}/read`,
            type: "GET",
            success: response => {
                if (response.status === "done") { // Controllo se terminare il polling
                    clearInterval(polling); // Termino il polling
                    $("#newReadButton").prop("disabled", false); // Abilito il pulsante
                    $("#recordingLogo").addClass("hidden"); // Nascondo logo recording
                    $("#successReadLogo").removeClass("hidden"); // Visualizzo messaggio success
                    return;
                }

                // Parsing del CSV
                parseCSV(response.data);
            }, error: error => {
                console.error(error);
            }
        });
    }, 1000); // Ogni secondo
};

// Faccio il parsing del CSV
const parseCSV = text => {
    let rows, reference;
    try {
        rows = text.split(";").map(row => row.split(","));
        reference = parseFloat(rows[0]); // Valore di riferimento
        rows.shift(); // Rimuovo header con valore di riferimento
    } catch (e) {
        console.error("Errore durante la pulizia e suddivisione del CSV:", e);
        return;
    }

    // Calcolo massimi, minimi e differenze da valore di riferimento
    let maxX, maxY, distanceFromReference, leftSeries, rightSeries;
    try {
        maxX = trovaMassimo(rows.map(item => item[0])); // Trovo valore massimo
        maxY = trovaMassimo(rows.map(item => item[1])); // trovo valore minimo

        // Calcolo la differenza con il valore di riferimento
        distanceFromReference = rows.map(item => { return { original: parseFloat(item[2]), difference: parseFloat(item[2]) - reference }; });
        leftSeries = distanceFromReference.filter(item => item.difference < 0); // Minori di 0 => Serie di sinistra (Blu)
        rightSeries = distanceFromReference.filter(item => item.difference >= 0); // Maggiori di 0 => Serie di destra (Rosso)
    } catch (e) {
        console.error("Errore durante il calcolo di massimi, minimi e differenze:", e);
        return;
    }

    // Normalizzo le serie
    try {
        leftSeries = normalizeSerires(leftSeries); // Normalizzo la serie di sinistra
        rightSeries = normalizeSerires(rightSeries); // Normalizzo la serie di destra
    } catch (e) {
        console.error("Errore durante la normalizzazione delle serie:", e);
        return;
    }

    // Creo oggetti classe Rainbow
    const rainbowBlue = new Rainbow();
    const rainbowRed = new Rainbow();
    try {
        rainbowBlue.setNumberRange(1, 100);
        rainbowBlue.setSpectrum("white", "blue");
        rainbowRed.setNumberRange(1, 100);
        rainbowRed.setSpectrum("white", "red");
    } catch (e) {
        console.error("Errore durante la creazione degli oggetti Rainbow:", e);
    }

    // Ciclo le righe e creo mapping
    const dataMap = new Map();
    try {
        rows.forEach(row => {
            const x = parseInt(row[0]);
            const y = parseInt(row[1]);
            let mag = parseFloat(row[2]), magNorm, color;
            if (mag < reference) { // Prendo i valori normalizzati
                magNorm = parseInt(leftSeries.filter(item => item.original == mag)[0].absDifferenceNorm) || 0;
                color = rainbowBlue.colourAt(magNorm); // Prendo la gradazione giusta
            } else {
                magNorm = parseInt(rightSeries.filter(item => item.original == mag)[0].absDifferenceNorm) || 0;
                color = rainbowRed.colourAt(magNorm); // Prendo la gradazione giusta
            }

            // Aggiungo a mapping
            dataMap.set(`${x},${y}`, `#${color}`);
        });
    } catch (e) {
        console.error("Errore durante la creazione del mapping:", e);
    }

    // Creo e aggiungo tabella alla view
    try {
        let table = createTable(maxX, maxY, dataMap);
        table = fillBlanks(table);
        const tableContainer = document.getElementById("tableContainer");
        tableContainer.innerHTML = ""; // Pulisco il contenuto esistente
        tableContainer.appendChild(table); // Aggiungo la nuova tabella
    } catch (e) {
        console.error("Errore durante la creazione della tabella:", e);
    }
};

// Normalizzo i valori delle serie
const normalizeSerires = series => {
    let absSeries = series.map(item => { return { original: item.original, difference: item.difference, absDifference: Math.abs(item.difference) }; }); // Aggiungo il valore assoluto
    const max = trovaMassimo(absSeries.map(item => item.absDifference));
    const min = trovaMinimo(absSeries.map(item => item.absDifference));
    return absSeries.map(item => { return { original: item.original, difference: item.difference, absDifference: item.absDifference, absDifferenceNorm: normalizzaValore(item.absDifference, min, max) }; });
};

// Creo la tabella
const createTable = (width, height, dataMap) => {
    const table = document.createElement("table");
    table.style.borderCollapse = "collapse";

    // Ciclo le righe e le colonne, creo le celle e le coloro
    for (let y = 0; y <= height; y++) {
        const tr = document.createElement("tr");
        for (let x = 0; x <= width; x++) {
            const td = document.createElement("td");
            td.style.border = tableCellBorder;
            td.style.width = tableCellWidth;
            td.style.height = tableCellHeight;
            const key = `${x},${y}`;
            if (dataMap.has(key))
                td.style.backgroundColor = dataMap.get(key); // Prendo colore
            tr.appendChild(td);
        }
        table.appendChild(tr);
    }

    return table;
};

// Normalizzo un valore tra 0 e 100
const normalizzaValore = (x, minValue, maxValue) => {
    return ((x - minValue) / (maxValue - minValue)) * 100;
};

// Trovo il massimo in un array
const trovaMassimo = arr => {
    return arr.length === 0 ? undefined : Math.max(...arr);
};

// Trovo il minimo in un array
const trovaMinimo = arr => {
    return arr.length === 0 ? undefined : Math.min(...arr);
};

// Riempio gli spazi bianchi
const fillBlanks = table => {
    const numeroRighe = table.rows.length;
    const numeroColonne = table.rows[0] ? table.rows[0].cells.length : 0;
    for (let y = 0; y < numeroRighe; y++) { // Ciclo righe
        for (let x = 0; x < numeroColonne; x++) { // Ciclo colonne
            if (!table.rows[y].cells[x].style.backgroundColor)
                table.rows[y].cells[x].style.backgroundColor = getColorAverage(x, y, table.rows);
        }
    }
    return table;
};

// Calcola la media dei colori intorno alla cella
const getColorAverage = (x, y, rows) => {
    let r = 0, g = 0, b = 0;
    let count = 0;
    for (let i = x - 1; i <= x + 1; i++) {
        for (let j = y - 1; j <= y + 1; j++) {
            if (i >= 0 && i < rows[0].cells.length && j >= 0 && j < rows.length) {
                const cell = rows[j].cells[i];
                if (cell.style.backgroundColor) {
                    const rgb = cell.style.backgroundColor.match(/\d+/g);
                    r += parseInt(rgb[0], 10);
                    g += parseInt(rgb[1], 10);
                    b += parseInt(rgb[2], 10);
                    count++;
                }
            }
        }
    }

    if (count > 0) return `rgb(${Math.round(r / count)}, ${Math.round(g / count)}, ${Math.round(b / count)})`;
};