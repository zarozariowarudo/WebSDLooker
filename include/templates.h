#pragma once

inline const char* htmlTemplate = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 SD File Manager</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <h2>📂 ESP32 SD File Manager</h2>
        
        <div class="upload-box">
            <h3>Загрузить файл на SD-карту</h3>
            <input type="file" id="fileInput"><br>
            <button onclick="uploadFile()">Загрузить</button>
            <div id="status"></div>
        </div>
 
        <table>
            <thead>
                <tr>
                    <th>Путь к файлу</th>
                    <th>Размер</th>
                    <th>Действия</th>
                </tr>
            </thead>
            <tbody id="fileTable"></tbody>
        </table>
    </div>
 
    <script src="/script.js"></script>
</body>
</html>
)rawliteral";

inline const char* cssStyle = 
R"rawliteral(
    * { box-sizing: border-box; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
    .container { max-width: 900px; margin: auto; background: #1e1e1e; padding: 25px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.6); }

    h2 { text-align: center; color: #4CAF50; margin-top: 0; }

    .upload-box { background: #2a2a2a; padding: 20px; border-radius: 8px; margin-bottom: 25px; border: 1px solid #3d3d3d; }

    input[type="file"] { margin-bottom: 12px; color: #ccc; }

    button { background: #4CAF50; color: white; border: none; padding: 9px 15px; cursor: pointer; border-radius: 6px; font-weight: bold; transition: 0.2s; margin-right: 5px; }
    
    button:hover { background: #45a049; }
    .btn-danger { background: #f44336; }
    .btn-danger:hover { background: #d32f2f; }
    .btn-rename { background: #2196F3; }
    .btn-rename:hover { background: #0b7dda; }

    table { width: 100%; border-collapse: collapse; margin-top: 10px; }

    th, td { padding: 12px; border-bottom: 1px solid #333; text-align: left; }
    th { background: #282828; color: #aaa; }

    tr:hover { background: #252525; }
    
    a { color: #81c784; text-decoration: none; word-break: break-all; }

    a:hover { text-decoration: underline; }

    #status { font-weight: bold; margin-top: 10px; }
)rawliteral";

inline const char* jsScript = 
R"rawliteral(
    function loadFiles() {
        fetch('/list').then(res => res.json()).then(files => {
            let html = '';
            if(files.length === 0) {
                html = '<tr><td colspan="3" style="text-align:center;">Файлов нет (или SD пуста)</td></tr>';
            }
            files.forEach(f => {
                let sizeStr = (f.size / 1024 / 1024).toFixed(2) + ' MB';
                if (f.size < 1024 * 1024) sizeStr = (f.size / 1024).toFixed(1) + ' KB';
                let warn = f.corrupted ? ' ⚠️ имя повреждено (шум SPI)' : '';
                
                html += `<tr>
                    <td><a href="/download?file=${encodeURIComponent(f.name)}" download>${f.name}</a>${warn}</td>
                    <td>${sizeStr}</td>
                    <td>
                        <button class="btn-rename" onclick="renameFile('${f.name}')">Переименовать</button>
                        <button class="btn-danger" onclick="deleteFile('${f.name}')">Удалить</button>
                    </td>
                </tr>`;
            });
            document.getElementById('fileTable').innerHTML = html;
        }).catch(err => {
            document.getElementById('fileTable').innerHTML =
                '<tr><td colspan="3" style="color:#f44336;">Ошибка загрузки списка файлов: ' + err + '</td></tr>';
            console.error('Ошибка /list:', err);
        });
    }

    function uploadFile() {
        let fileInput = document.getElementById("fileInput");
        let file = fileInput.files[0];
        if (!file) return alert("Выберите файл!");
        
        let formData = new FormData();
        formData.append("file", file, "/" + file.name);
        
        let status = document.getElementById("status");
        status.style.color = "#ffeb3b";
        status.innerText = "Идет загрузка... Не закрывайте страницу.";
        
        fetch('/upload', { method: 'POST', body: formData })
            .then(res => {
                status.style.color = "#4CAF50";
                status.innerText = "Загружено успешно!";
                fileInput.value = "";
                loadFiles();
            })
            .catch(() => {
                status.style.color = "#f44336";
                status.innerText = "Ошибка при загрузке.";
            });
    }

    function deleteFile(path) {
        if (confirm("Удалить файл " + path + "?")) {
            fetch('/delete?file=' + encodeURIComponent(path), { method: 'DELETE' })
                .then(() => loadFiles());
        }
    }

    function renameFile(oldPath) {
        let newPath = prompt("Введи новый путь (с / в начале):", oldPath);
        if (newPath && newPath !== oldPath) {
            fetch('/rename?from=' + encodeURIComponent(oldPath) + '&to=' + encodeURIComponent(newPath), { method: 'POST' })
                .then(() => loadFiles());
        }
    }

    window.onload = loadFiles;
)rawliteral";