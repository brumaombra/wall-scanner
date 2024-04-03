// Parametri globali
const tableCellWidth = "30px";
const tableCellHeight = "30px";
const tableCellBorder = "1px solid #34495e";
const ESP32IP = ""; // "http://localhost:3000"
let valuesVisible = ""; // Visibilità dei valori di magnetismo ("", "MAG", "NORM")
const scanStatus = { READY: 0, TUNING: 1, SCANNING: 2, ENDED: 3 }; // Stato della scansione
let currentStatus = scanStatus.READY; // Stato attuale della scansione

// Documento pronto
$(document).ready(() => {
    $("#linkIconOk").animate({ opacity: 0 }, 0); // Nascondo icona link OK
    init(); // Funzione init
});

// Funzione init
const init = () => {
    setTimeout(() => {
        initSocket(); // Inizializzo il WebSocket
        setTimeout(() => {
            getConfuguration(() => { // Prendo la configurazione iniziale
                setBusy(false); // Busy off
            }, () => {
                setBusy(false); // Busy off
            });
        }, 1000);
    }, 1000);
};

// Inizializzo il WebSocket
const initSocket = () => {
    // const socketUrl = `ws://${window.location.host}/ws`; // URL per connettersi al WebSocket
    const socketUrl = `ws://localhost:8080`; // URL per connettersi al WebSocket
    let socket = new WebSocket(socketUrl); // Oggetto WebSocket
    socket.onopen = event => { // Apertura connessione
        console.log("Connessione al WebSocket aperta");
        showHideLinkIcon(true); // Mostro l'icona di collegamento
    };
    socket.onclose = event => { // Chiusura connessione
        console.log(`Connessione chiusa, codice errore: ${event.code}, motivo: ${event.reason}`);
        showHideLinkIcon(false); // Nascondo l'icona di collegamento
    };
    socket.onerror = error => { // Errore
        console.log(`Errore WebSocket: ${error.message}`);
    };
    socket.onmessage = event => { // Evento di ricezione di un messaggio
        const json = JSON.parse(event.data); // Faccio il parse della stringa JSON
        manageSocketMessage(json); // Gestisco il messaggio di risposta
    };
};

// Prendo la configurazione iniziale
const getConfuguration = (successCallback, errorCallback) => {
    fetch(`${ESP32IP}/getSettings`).then(response => {
        return response.json(); // Prendo il JSON
    }).then(data => {
        $("#settingsResolution").val(data.resolution || "3"); // Imposto risoluzione scansione
        successCallback(); // Success callback
    }).catch(error => {
        showToast("ERROR", "Errore: impostazioni non caricate"); // Mostro toast
        console.log(error);
        errorCallback(); // Error callback
    });
};

// Salvo le impostazioni
const handleSaveSettingsPress = () => {
    setBusy(true); // Busy on
    const queryString = new URLSearchParams({ // Creo la query string
        resolution: $("#settingsResolution").val() || ""
    }).toString();
    fetch(`${ESP32IP}/setSettings?${queryString}`).then(response => {
        return response.json(); // Prendo il JSON
    }).then(data => {
        setBusy(false); // Busy off
        valuesVisible = $("#settingsValues").val(); // Salvo impostazione visibilità valori
        showToast("SUCCESS", "Impostazioni salvate!"); // Mostro toast
    }).catch(error => {
        setBusy(false); // Busy off
        showToast("ERROR", "Errore durante il salvataggio"); // Mostro toast
        console.log(error);
    });
};

/*********************************** Gestione WebSocket ***********************************/

// Gestisco il messaggio di risposta
manageSocketMessage = data => {
    switch (data.status) {
        case scanStatus.READY: // L'ESP è in attesa di iniziare una scansione
            manageStatusReady(data);
            break;
        case scanStatus.TUNING: // L'ESP sta eseguendo la calibrazione
            manageStatusTuning(data);
            break;
        case scanStatus.SCANNING: // L'ESP sta eseguendo una scansione
            manageStatusScanning(data);
            break;
        case scanStatus.ENDED: // L'ESP ha terminato una scansione
            manageStatusEnded(data);
            break;
    }
};

// Gestione dello stato READY
const manageStatusReady = response => {
    if (response.status === this.currentStatus) return; // Se è già uguale esco
    this.currentStatus = response.status || scanStatus.READY; // Aggiorno stato
    hideEveryContainer(); // Nascondo tutti i container
    $("#scansionePlaceholder").removeClass("hidden"); // Visualizzo placeholder
    $("#settingsButton").prop("disabled", false); // Abilito il pulsante impostazioni
};

// Gestione dello stato TUNING
const manageStatusTuning = response => {
    let value = response.data?.toString() || "0"; // Valore di taratura attuale
    if (value.includes(";")) { // Se contiene la virgola vuol dire che è il valore di riferimento definitivo
        value = value.split(";")[0]; // Prendo solo il primo valore
        $("#tuningIcon").addClass("text-success"); // Aggiungo colore verde
        $("#tuningValue").addClass("text-success"); // Aggiungo colore verde
    } else {
        $("#tuningIcon").removeClass("text-success"); // Tolgo verde
        $("#tuningValue").removeClass("text-success"); // Tolgo verde
    }

    // Imposto valore
    $("#tuningValue").text(`${value} ms`);
    if (response.status === this.currentStatus) return; // Se è già uguale esco
    this.currentStatus = response.status || scanStatus.READY; // Aggiorno stato
    hideEveryContainer(); // Nascondo tutti i container
    $("#tuningContainer").removeClass("hidden"); // Visualizzo container
    $("#settingsButton").prop("disabled", true); // Disabilito il pulsante impostazioni
};

// Gestione dello stato SCANNING
const manageStatusScanning = response => {
    parseCSV(response.data); // Parsing del CSV
    if (response.status === this.currentStatus) return; // Se è già uguale esco
    this.currentStatus = response.status || scanStatus.READY; // Aggiorno stato
    hideEveryContainer(); // Nascondo tutti i container
    $("#scansioneContainer").removeClass("hidden"); // Visualizzo container
    $("#recordingLogo").removeClass("hidden"); // Visualizzo logo recording
    $("#settingsButton").prop("disabled", true); // Disabilito il pulsante impostazioni    
};

// Gestione dello stato ENDED
const manageStatusEnded = response => {
    if (response.status === this.currentStatus) return; // Se è già uguale esco
    this.currentStatus = response.status || scanStatus.READY; // Aggiorno stato
    parseCSV(response.data); // Parsing del CSV
    hideEveryContainer(); // Nascondo tutti i container
    $("#scansioneContainer").removeClass("hidden"); // Visualizzo container
    $("#successReadLogo").removeClass("hidden"); // Visualizzo messaggio success
    $("#settingsButton").prop("disabled", false); // Abilito il pulsante impostazioni
};

// Nascondo tutti i container
const hideEveryContainer = () => {
    $("#tuningContainer").addClass("hidden"); // Nascondo container
    $("#scansioneContainer").addClass("hidden"); // Nascondo container scansione
    $("#scansionePlaceholder").addClass("hidden"); // Nascondo placeholder
    $("#recordingLogo").addClass("hidden"); // Nascondo logo recording
    $("#successReadLogo").addClass("hidden"); // Nascondo logo success
};

/*********************************** Creazione della heatmap ***********************************/

// Faccio il parsing del CSV
const parseCSV = text => {
    let rows, reference;
    try {
        rows = text.split(";").map(row => row.split(","));
        reference = parseFloat(rows[0] || 0); // Valore di riferimento
        rows = rows.filter(item => item.length === 3); // Prendo solo righe valide (Con i 3 valori)
        if (rows.length === 0) return; // Se vuoto esco
    } catch (e) {
        console.log("Errore durante la pulizia e suddivisione del CSV:", e);
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
        console.log("Errore durante il calcolo di massimi, minimi e differenze:", e);
        return;
    }

    // Normalizzo le serie
    try {
        leftSeries = normalizeSerires(leftSeries); // Normalizzo la serie di sinistra
        rightSeries = normalizeSerires(rightSeries); // Normalizzo la serie di destra
    } catch (e) {
        console.log("Errore durante la normalizzazione delle serie:", e);
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
        console.log("Errore durante la creazione degli oggetti Rainbow:", e);
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
            dataMap.set(`${x},${y}`, { color: `#${color}`, mag: mag, magNorm: magNorm });
        });
    } catch (e) {
        console.log("Errore durante la creazione del mapping:", e);
    }

    // Creo e aggiungo tabella alla view
    try {
        let table = createTable(maxX, maxY, dataMap);
        table = fillBlanks(table);
        const tableContainer = document.getElementById("tableContainer");
        tableContainer.innerHTML = ""; // Pulisco il contenuto esistente
        tableContainer.appendChild(table); // Aggiungo la nuova tabella
    } catch (e) {
        console.log("Errore durante la creazione della tabella:", e);
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
            if (dataMap.has(key)) {
                td.style.backgroundColor = dataMap.get(key).color; // Prendo colore
                if (valuesVisible !== "") { // Se impostazione attiva, aggiungo valori
                    td.style.color = getContrastColor(td.style.backgroundColor); // Prendo bianco o nero
                    td.textContent = valuesVisible === "MAG" ? parseInt(dataMap.get(key).mag) : dataMap.get(key).magNorm; // Prendo il valore
                }
            }
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

// Calcola il colore con contrasto elevato
const getContrastColor = bgColor => {
    const [r, g, b] = bgColor.match(/\d+/g).map(Number); // Prendo R, G e B
    const luminance = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255; // Calcolo la luminosità
    return luminance > 0.5 ? "#000000" : "#FFFFFF"; // Bianco o nero
};

// Busy
const setBusy = busy => {
    if (busy) { // Visualizzo busy
        $("#fullScreenBusy").removeClass("hidden");
        $("#fullScreenBusy").animate({ opacity: 1 }, 200, "swing");
    } else { // Nascondo busy
        $("#fullScreenBusy").animate({ opacity: 0 }, 200, "swing", () => {
            $("#fullScreenBusy").addClass("hidden");
        });
    }
};

// Visualizzo o nascondo icona connessione
const showHideLinkIcon = connected => {
    if (connected) { // Mostro icona connessione OK
        $("#linkIconKo").animate({ opacity: 0 }, 200, "swing", () => {
            $("#linkIconOk").removeClass("hidden");
            $("#linkIconOk").animate({ opacity: 1 }, 200, "swing");
        });
    } else { // Mostro icona connessione KO
        $("#linkIconOk").animate({ opacity: 0 }, 200, "swing", () => {
            $("#linkIconOk").addClass("hidden");
            $("#linkIconKo").animate({ opacity: 1 }, 200, "swing");
        });
    }
};

// Visualizzo il messaggio toast
const showToast = (type, message) => {
    if (type === "SUCCESS") { // Mostro icona di success
        $("#messageToastSuccessIcon").removeClass("hidden");
        $("#messageToastErrorIcon").addClass("hidden");
    } else { // Mostro icona di errore
        $("#messageToastSuccessIcon").addClass("hidden");
        $("#messageToastErrorIcon").removeClass("hidden");
    }
    $("#messageToastText").text(message); // Imposto il testo
    $("#messageToast").toast({ delay: 2000 }).toast("show"); // Visualizzo il toast
};