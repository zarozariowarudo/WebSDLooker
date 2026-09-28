#pragma once

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