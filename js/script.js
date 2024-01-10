// Parametri globali
const tableCellWidth = "30px";
const tableCellHeight = "30px";
const tableCellBorder = "1px solid black";

// Carico il CSV
const loadCSV = event => {
    const file = event.target.files[0];
    if (!file) return; // Se vuoto esco
    const reader = new FileReader();
    reader.onload = e => {
        parseCSV(e.target.result || "");
    };
    reader.readAsText(file);
}

// Faccio il parsing del CSV
const parseCSV = text => {
    text = text.replace("\r", ""); // Pulisco i dati sporchi
    let rows = text.split("\n").map(row => row.split(","));
    rows.shift(); // Rimuovo header

    // Trovo massimi e minimi
    let maxX = trovaMassimo(rows.map(item => item[0]));
    let maxY = trovaMassimo(rows.map(item => item[1]));
    let maxMag = trovaMassimo(rows.map(item => item[2]));
    let minX = trovaMinimo(rows.map(item => item[0]));
    let minY = trovaMinimo(rows.map(item => item[1]));
    let minMag = trovaMinimo(rows.map(item => item[2]));

    // Normalizzo i dati partendo da 0
    const differenceX = minX < 0 ? 0 - minX : 0;
    const differenceY = minY < 0 ? 0 - minY : 0;
    maxX += differenceX;
    maxY += differenceY;

    // Creo oggetto mapping
    const dataMap = new Map();
    rows.forEach(row => {
        // Aggiungo differenza ai due assi per arrivare a 0
        let x = parseInt(row[0]);
        let y = parseInt(row[1]);
        x += differenceX;
        y += differenceY;

        // Normalizzo mag
        const mag = parseFloat(row[2]);
        const normalizedMag = normalizzaValore(mag, minMag, maxMag);

        // Aggiungo a mapping
        dataMap.set(`${x},${y}`, normalizedMag);
    });

    // Creo e aggiungo tabella
    let table = createTable(maxX, maxY, dataMap);
    table = fillBlanks(table);
    const tableContainer = document.getElementById("tableContainer");
    tableContainer.innerHTML = ""; // Pulisco il contenuto esistente
    tableContainer.appendChild(table); // Aggiungo la nuova tabella
}

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
                td.style.backgroundColor = getColoreHeatMap(dataMap.get(key));
            tr.appendChild(td);
        }
        table.appendChild(tr);
    }

    return table;
}

// Normalizzo un valore tra 0 e 100
const normalizzaValore = (x, minValue, maxValue) => {
    return ((x - minValue) / (maxValue - minValue)) * 100;
}

// Genero un colore heatmap basato su un valore tra 0 e 100
const getColoreHeatMap = valore => {
    const rosso = valore * 255 / 100;
    const blu = 255 - rosso;
    return `rgb(${rosso}, 0, ${blu})`;
}

// Trovo il massimo in un array
const trovaMassimo = arr => {
    return arr.length === 0 ? undefined : Math.max(...arr);
}

// Trovo il minimo in un array
const trovaMinimo = arr => {
    return arr.length === 0 ? undefined : Math.min(...arr);
}

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
}

// Genero il colore usando gli elementi intorno
const getColorAverage = (x, y, rows) => {
    let somma = 0;
    let contatore = 0;
    for (let i = -1; i <= 1; i++) { // Ciclo i-1, i, i+1
        for (let j = -1; j <= 1; j++) { // Ciclo j-1, j, j+1
            if (rows[y + i] && rows[y + i].cells[x + j] && rows[y + i].cells[x + j].style.backgroundColor) {
                somma += parseInt(rows[y + i].cells[x + j].style.backgroundColor.replace("rgb(", "").replace(")", "").split(",")[0]);
                contatore++;
            }
        }
    }
    return `rgb(${somma / contatore}, 0, ${255 - somma / contatore})`;
}