// Global parameters
const tableCellWidth = "30px";
const tableCellHeight = "30px";
const tableCellBorder = "1px solid #34495e";
const ESP32IP = ""; // "http://localhost:3000"
const scanStatus = { READY: 0, TUNING: 1, SCANNING: 2, ENDED: 3 }; // Scan status
let currentStatus = scanStatus.READY; // Current scan status
const taraBobina = 29; // Coil tare value

// Document ready
$(document).ready(() => {
    $("#linkIconOk").animate({ opacity: 0 }, 0); // Hide link OK icon
    init(); // Init function
});

// Init function
const init = () => {
    setTimeout(() => {
        getConfuguration(() => { // Get initial configuration
            initSocket(); // Initialize WebSocket
            setBusy(false); // Busy off
        }, () => {
            initSocket(); // Initialize WebSocket
            setBusy(false); // Busy off
        });
    }, 1000);
};

// Initialize WebSocket
const initSocket = () => {
    const socketUrl = `ws://${window.location.host}/ws`; // URL to connect to WebSocket
    // const socketUrl = `ws://localhost:8080`; // URL to connect to WebSocket
    let socket = new WebSocket(socketUrl); // WebSocket object
    socket.onopen = event => { // Connection opening
        console.log("WebSocket connection opened");
        showHideLinkIcon(true); // Show connection icon
    };
    socket.onclose = event => { // Connection closing
        console.log(`Connection closed, error code: ${event.code}, reason: ${event.reason}`);
        showHideLinkIcon(false); // Hide connection icon
    };
    socket.onerror = error => { // Error
        console.log(`WebSocket error: ${error.message}`);
    };
    socket.onmessage = event => { // Message reception event
        try { // Handle parsing error
            const json = JSON.parse(event.data); // Parse JSON string
            manageSocketMessage(json); // Handle response message
        } catch (error) {
            console.log(error);
        }
    };
};

// Get initial configuration
const getConfuguration = (successCallback, errorCallback) => {
    fetch(`${ESP32IP}/getSettings`).then(response => {
        return response.json(); // Get JSON
    }).then(data => {
        $("#settingsResolution").val(data.resolution || "3"); // Set scan resolution
        $("#settingsNormalizzazione").val(data.normalize ? "ON" : "OFF"); // Set values normalization
        $("#settingsDisplayValues").val(data.displayValues ? "ON" : "OFF"); // Set values display
        successCallback(); // Success callback
    }).catch(error => {
        showToast("ERROR", "Error: settings not loaded"); // Show toast
        console.log(error);
        errorCallback(); // Error callback
    });
};

// Save settings
const handleSaveSettingsPress = () => {
    setBusy(true); // Busy on
    const queryString = new URLSearchParams({ // Create query string
        resolution: $("#settingsResolution").val() || "3",
        normalize: $("#settingsNormalizzazione").val() === "ON",
        displayValues: $("#settingsDisplayValues").val() === "ON"
    }).toString();
    fetch(`${ESP32IP}/setSettings?${queryString}`).then(response => {
        return response.json(); // Get JSON
    }).then(data => {
        setBusy(false); // Busy off
        showToast("SUCCESS", "Settings saved!"); // Show toast
    }).catch(error => {
        setBusy(false); // Busy off
        showToast("ERROR", "Error during saving"); // Show toast
        console.log(error);
    });
};

/*********************************** WebSocket Management ***********************************/

// Handle response message
manageSocketMessage = data => {
    switch (data.status) {
        case scanStatus.READY: // ESP is waiting to start a scan
            manageStatusReady(data);
            break;
        case scanStatus.TUNING: // ESP is performing calibration
            manageStatusTuning(data);
            break;
        case scanStatus.SCANNING: // ESP is performing a scan
            manageStatusScanning(data);
            break;
        case scanStatus.ENDED: // ESP has finished a scan
            manageStatusEnded(data);
            break;
    }
};

// Handle READY status
const manageStatusReady = response => {
    if (response.status === this.currentStatus) return; // If already equal exit
    this.currentStatus = response.status || scanStatus.READY; // Update status
    hideEveryContainer(); // Hide all containers
    $("#scansionePlaceholder").removeClass("hidden"); // Show placeholder
    $("#settingsButton").prop("disabled", false); // Enable settings button
    $("#tableContainer").html(""); // Empty table
};

// Handle TUNING status
const manageStatusTuning = response => {
    let value = response.data?.toString() || "0"; // Current tuning value
    if (value.includes(";")) { // If contains semicolon it means it's the final reference value
        value = value.split(";")[0]; // Take only the first value
        $("#tuningIcon").addClass("text-success"); // Add green color
        $("#tuningValue").addClass("text-success"); // Add green color
    } else {
        $("#tuningIcon").removeClass("text-success"); // Remove green
        $("#tuningValue").removeClass("text-success"); // Remove green
    }

    // Set value
    value = value / taraBobina * 100; // Normalize to 100
    value = value.toFixed(2); // Round to two decimal places
    $("#tuningValue").text(value); // Set value
    document.getElementById("sliderCalibrazione").value = value; // Set value on range
    if (response.status === this.currentStatus) return; // If already equal exit
    this.currentStatus = response.status || scanStatus.READY; // Update status
    hideEveryContainer(); // Hide all containers
    $("#tuningContainer").removeClass("hidden"); // Show container
    $("#settingsButton").prop("disabled", true); // Disable settings button
    $("#tableContainer").html(""); // Empty table
};

// Handle SCANNING status
const manageStatusScanning = response => {
    parseCSV(response.data); // Parse CSV
    if (response.status === this.currentStatus) return; // If already equal exit
    this.currentStatus = response.status || scanStatus.READY; // Update status
    hideEveryContainer(); // Hide all containers
    $("#scansioneContainer").removeClass("hidden"); // Show container
    $("#recordingLogo").removeClass("hidden"); // Show recording logo
    $("#settingsButton").prop("disabled", true); // Disable settings button    
};

// Handle ENDED status
const manageStatusEnded = response => {
    if (response.status === this.currentStatus) return; // If already equal exit
    this.currentStatus = response.status || scanStatus.READY; // Update status
    parseCSV(response.data); // Parse CSV
    hideEveryContainer(); // Hide all containers
    $("#scansioneContainer").removeClass("hidden"); // Show container
    $("#successReadLogo").removeClass("hidden"); // Show success message
    $("#settingsButton").prop("disabled", false); // Enable settings button
};

// Hide all containers
const hideEveryContainer = () => {
    $("#tuningContainer").addClass("hidden"); // Hide container
    $("#scansioneContainer").addClass("hidden"); // Hide scan container
    $("#scansionePlaceholder").addClass("hidden"); // Hide placeholder
    $("#recordingLogo").addClass("hidden"); // Hide recording logo
    $("#successReadLogo").addClass("hidden"); // Hide success logo
};

/*********************************** Heatmap Creation ***********************************/

// Parse CSV
const parseCSV = text => {
    let rows, reference, normalizeMagValues = $("#settingsNormalizzazione").val() === "ON";
    try {
        rows = text.split(";").map(row => row.split(","));
        reference = parseFloat(rows[0] || 0); // Reference value
        rows = rows.filter(item => item.length === 3); // Take only valid rows (with 3 values)
        if (rows.length === 0) return; // If empty exit
    } catch (e) {
        console.log("Error during CSV cleaning and splitting:", e);
        return;
    }

    // Calculate max, min and differences from reference value
    let maxX, maxY, distanceFromReference, leftSeries, rightSeries;
    try {
        maxX = trovaMassimo(rows.map(item => item[0])); // Find max value
        maxY = trovaMassimo(rows.map(item => item[1])); // Find min value

        // Calculate difference with reference value
        distanceFromReference = rows.map(item => { return { original: parseFloat(item[2]), difference: parseFloat(item[2]) - reference }; });
        leftSeries = distanceFromReference.filter(item => item.difference < 0); // Less than 0 => Left series (Blue)
        rightSeries = distanceFromReference.filter(item => item.difference >= 0); // Greater than 0 => Right series (Red)
    } catch (e) {
        console.log("Error during calculation of max, min and differences:", e);
        return;
    }

    // Normalize series and add difference values
    try {
        leftSeries = normalizeSerires(leftSeries); // Normalize left series
        rightSeries = normalizeSerires(rightSeries); // Normalize right series
    } catch (e) {
        console.log("Error during series normalization:", e);
        return;
    }

    // Create Rainbow class objects
    const rainbowBlue = new Rainbow();
    const rainbowRed = new Rainbow();
    try {
        rainbowBlue.setNumberRange(1, normalizeMagValues ? 100 : 10);
        rainbowBlue.setSpectrum("white", "blue");
        rainbowRed.setNumberRange(1, normalizeMagValues ? 100 : 10);
        rainbowRed.setSpectrum("white", "red");
    } catch (e) {
        console.log("Error during Rainbow objects creation:", e);
    }

    // Loop rows and create mapping
    const dataMap = new Map();
    try {
        rows.forEach(row => {
            const x = parseInt(row[0]);
            const y = parseInt(row[1]);
            let mag = parseFloat(row[2]), absDifferenceValue, color;
            if (mag < reference) { // Take values
                if (normalizeMagValues) // Normalized or not
                    absDifferenceValue = parseInt(leftSeries.filter(item => item.original == mag)[0].absDifferenceNorm) || 0;
                else
                    absDifferenceValue = parseInt(leftSeries.filter(item => item.original == mag)[0].absDifference) || 0;
                color = rainbowBlue.colourAt(absDifferenceValue); // Take correct shade
            } else {
                if (normalizeMagValues) // Normalized or not
                    absDifferenceValue = parseInt(rightSeries.filter(item => item.original == mag)[0].absDifferenceNorm) || 0;
                else
                    absDifferenceValue = parseInt(rightSeries.filter(item => item.original == mag)[0].absDifference) || 0;
                color = rainbowRed.colourAt(absDifferenceValue); // Take correct shade
            }

            // Add to mapping
            dataMap.set(`${x},${y}`, { color: `#${color}`, mag: mag, absDifferenceValue: absDifferenceValue });
        });
    } catch (e) {
        console.log("Error during mapping creation:", e);
    }

    // Create and add table to view
    try {
        let table = createTable(maxX, maxY, dataMap);
        table = fillBlanks(table);
        const tableContainer = document.getElementById("tableContainer");
        tableContainer.innerHTML = ""; // Clear existing content
        tableContainer.appendChild(table); // Add new table
    } catch (e) {
        console.log("Error during table creation:", e);
    }
};

// Normalize series values
const normalizeSerires = series => {
    let absSeries = series.map(item => { return { original: item.original, difference: item.difference, absDifference: Math.abs(item.difference) }; }); // Add absolute value
    const max = trovaMassimo(absSeries.map(item => item.absDifference));
    const min = trovaMinimo(absSeries.map(item => item.absDifference));
    return absSeries.map(item => { return { original: item.original, difference: item.difference, absDifference: item.absDifference, absDifferenceNorm: normalizzaValore(item.absDifference, min, max) }; });
};

// Create table
const createTable = (width, height, dataMap) => {
    const table = document.createElement("table");
    table.style.borderCollapse = "collapse";

    // Loop rows and columns, create cells and color them
    for (let y = 0; y <= height; y++) {
        const tr = document.createElement("tr");
        for (let x = 0; x <= width; x++) {
            const td = document.createElement("td");
            td.style.border = tableCellBorder;
            td.style.width = tableCellWidth;
            td.style.height = tableCellHeight;
            const key = `${x},${y}`;
            if (dataMap.has(key)) {
                td.style.backgroundColor = dataMap.get(key).color; // Take color
                if ($("#settingsDisplayValues").val() === "ON") { // If setting active, add values
                    td.style.color = getContrastColor(td.style.backgroundColor); // Take white or black
                    td.textContent = parseInt(dataMap.get(key).mag); // Take value
                }
            }
            tr.appendChild(td);
        }
        table.appendChild(tr);
    }

    return table;
};

// Normalize a value between 0 and 100
const normalizzaValore = (x, minValue, maxValue) => {
    return ((x - minValue) / (maxValue - minValue)) * 100;
};

// Find max in array
const trovaMassimo = arr => {
    return arr.length === 0 ? undefined : Math.max(...arr);
};

// Find min in array
const trovaMinimo = arr => {
    return arr.length === 0 ? undefined : Math.min(...arr);
};

// Fill blank spaces
const fillBlanks = table => {
    const numeroRighe = table.rows.length;
    const numeroColonne = table.rows[0] ? table.rows[0].cells.length : 0;
    for (let y = 0; y < numeroRighe; y++) { // Loop rows
        for (let x = 0; x < numeroColonne; x++) { // Loop columns
            if (!table.rows[y].cells[x].style.backgroundColor)
                table.rows[y].cells[x].style.backgroundColor = getColorAverage(x, y, table.rows);
        }
    }
    return table;
};

// Calculate average of colors around the cell
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

// Calculate high contrast color
const getContrastColor = bgColor => {
    const [r, g, b] = bgColor.match(/\d+/g).map(Number); // Take R, G and B
    const luminance = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255; // Calculate luminance
    return luminance > 0.5 ? "#000000" : "#FFFFFF"; // White or black
};

// Busy
const setBusy = busy => {
    if (busy) { // Show busy
        $("#fullScreenBusy").removeClass("hidden");
        $("#fullScreenBusy").animate({ opacity: 1 }, 200, "swing");
    } else { // Hide busy
        $("#fullScreenBusy").animate({ opacity: 0 }, 200, "swing", () => {
            $("#fullScreenBusy").addClass("hidden");
        });
    }
};

// Show or hide connection icon
const showHideLinkIcon = connected => {
    if (connected) { // Show connection OK icon
        $("#linkIconKo").animate({ opacity: 0 }, 200, "swing", () => {
            $("#linkIconOk").removeClass("hidden");
            $("#linkIconOk").animate({ opacity: 1 }, 200, "swing");
        });
    } else { // Show connection KO icon
        $("#linkIconOk").animate({ opacity: 0 }, 200, "swing", () => {
            $("#linkIconOk").addClass("hidden");
            $("#linkIconKo").animate({ opacity: 1 }, 200, "swing");
        });
    }
};

// Show toast message
const showToast = (type, message) => {
    if (type === "SUCCESS") { // Show success icon
        $("#messageToastSuccessIcon").removeClass("hidden");
        $("#messageToastErrorIcon").addClass("hidden");
    } else { // Show error icon
        $("#messageToastSuccessIcon").addClass("hidden");
        $("#messageToastErrorIcon").removeClass("hidden");
    }
    $("#messageToastText").text(message); // Set text
    $("#messageToast").toast({ delay: 2000 }).toast("show"); // Show toast
};