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
            <div class="FileUploadBox">
                <h3>Загрузить файл на SD-карту</h3>
                <input type="file" id="fileInput">
                <button onclick="uploadFile()">Загрузить</button>
                <div id="status1"></div>
            </div>
            <div class="FolderUploadBox">
                <h3>Добавить папку на SD-карту</h3>
                <input type="text" id="folderInput"
                placeholder="Имя новой папки...">
                <button onclick="uploadFolder()">Добавить</button>
                <div id="status2"></div>
            </div>
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
