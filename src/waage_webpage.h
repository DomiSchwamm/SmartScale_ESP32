// Webseite im Flash-Speicher (PROGMEM) abgelegt. Vollständig in einer Datei, 
// da der Mikrocontroller-Hotspot über keinen Internetzugang verfügt.
#ifndef WAAGE_WEBPAGE_H
#define WAAGE_WEBPAGE_H

#include <pgmspace.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SmartScale - Lebensmittel</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f9; color: #333; }
        h1 { color: #2c3e50; }
        .card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; }
        input, select, button { padding: 10px; margin: 5px 0; width: 100%; box-sizing: border-box; }
        button { background-color: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        button:hover { background-color: #218838; }
        .btn-secondary { background-color: #007bff; }
        .btn-secondary:hover { background-color: #0069d9; }
        .del-btn { background-color: #dc3545; padding: 5px 10px; width: auto; font-size: 14px; }
        .del-btn:hover { background-color: #c82333; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { border: 1px solid #ddd; padding: 10px; text-align: left; }
        th { background-color: #f8f9fa; }
    </style>
</head>
<body>
    <h1>SmartScale</h1>
    
    <div class="card">
        <h3>Neues Lebensmittel hinzuf&uuml;gen</h3>
        <input type="text" id="name" placeholder="Name (z.B. Apfel oder Milchschnitte)" maxlength="30">
        <input type="number" id="kh" step="0.1" placeholder="Kohlenhydrate (g)">
        
        <select id="typ">
            <option value="100g">pro 100g (wird gewogen)</option>
            <option value="stueck">pro St&uuml;ck (wird gez&auml;hlt)</option>
        </select>

        <button onclick="hinzufuegen()">Speichern</button>
    </div>

    <div class="card">
        <h3>Sicherung &amp; Wiederherstellung</h3>
        <p>Sichere deine Lebensmittel-Liste oder lade eine gesicherte Datei hoch:</p>
        <button class="btn-secondary" onclick="window.location.href='/api/backup'">Sicherung herunterladen (.json)</button>
        <br><br>
        <input type="file" id="restoreFile" accept=".json">
        <button class="btn-secondary" onclick="wiederherstellen()">Sicherung einspielen</button>
    </div>

    <div class="card">
        <h3>Gespeicherte Lebensmittel</h3>
        <table id="tabelle">
            <thead>
                <tr>
                    <th>Name</th>
                    <th>KH-Wert</th>
                    <th>Einheit</th>
                    <th>Aktion</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
    </div>

    <script>
        // Liste holen und Tabelle neu aufbauen. textContent statt innerHTML,
        // damit ein Name nie als HTML gelesen wird.
        function ladeDaten() {
            fetch('/api/lebensmittel')
                .then(r => r.json())
                .then(data => {
                    const tbody = document.querySelector('#tabelle tbody');
                    tbody.innerHTML = '';

                    data.forEach(item => {
                        // der gibt es nur am Geraet
                        if (item.name === '? Manuell') return;

                        const tr = document.createElement('tr');
                        const texte = [
                            item.name,
                            item.kh + ' g',
                            item.stueck ? 'pro Stück' : 'pro 100g'
                        ];
                        texte.forEach(text => {
                            const td = document.createElement('td');
                            td.textContent = text;
                            tr.appendChild(td);
                        });

                        const btn = document.createElement('button');
                        btn.className = 'del-btn';
                        btn.textContent = 'Löschen';
                        btn.addEventListener('click', () => loescheEintrag(item.name));
                        const tdAktion = document.createElement('td');
                        tdAktion.appendChild(btn);
                        tr.appendChild(tdAktion);

                        tbody.appendChild(tr);
                    });
                });
        }

        // Formular absenden und Tabelle aktualisieren.
        // Gibt es den Namen schon, wird der Eintrag aktualisiert.
        function hinzufuegen() {
            let name = document.getElementById('name').value;
            let kh = document.getElementById('kh').value;
            let typ = document.getElementById('typ').value;
            if(!name || !kh) return alert('Bitte alle Felder ausfüllen!');

            fetch('/api/add', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `name=${encodeURIComponent(name)}&kh=${encodeURIComponent(kh)}&typ=${encodeURIComponent(typ)}`
            }).then(r => {
                if (!r.ok) {
                    alert('Eintrag konnte nicht gespeichert werden. Bitte Name und KH-Wert prüfen.');
                    return;
                }
                document.getElementById('name').value = '';
                document.getElementById('kh').value = '';
                ladeDaten();
            });
        }

        // Eintrag nach Rueckfrage loeschen
        function loescheEintrag(name) {
            if (confirm('Möchtest du "' + name + '" wirklich löschen?')) {
                fetch('/api/delete?name=' + encodeURIComponent(name), { method: 'DELETE' })
                    .then(() => ladeDaten());
            }
        }

        // JSON-Datei hochladen
        function wiederherstellen() {
            let fileInput = document.getElementById('restoreFile');
            if(fileInput.files.length === 0) return alert('Bitte eine .json-Datei auswählen!');

            let formData = new FormData();
            formData.append('data', fileInput.files[0]);

            fetch('/api/restore', {
                method: 'POST',
                body: formData
            }).then(r => {
                if(r.ok) {
                    alert('Sicherung erfolgreich eingespielt!');
                    fileInput.value = '';
                    ladeDaten();
                } else {
                    alert('Fehler beim Einspielen der Sicherung.');
                }
            });
        }

        // beim Laden gleich die Liste anzeigen
        ladeDaten();
    </script>
</body>
</html>
)rawliteral";

#endif
