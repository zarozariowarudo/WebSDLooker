#pragma once

inline const char* jsScript = R"rawliteral(
// Храним текущий путь, где находится пользователь (по умолчанию корень)
let currentDir = "/";

// 1. Главная функция: загружает содержимое папки с ESP32
function loadFolder(path) {
    currentDir = path;

    // Обновляем текст с текущим путем на странице (если есть такой элемент)
    let pathElement = document.getElementById("currentPath");
    if (pathElement) {
        pathElement.innerText = currentDir;
    }

    // Делаем запрос к нашему новому C++ хендлеру /list?dir=...
    fetch('/list?dir=' + encodeURIComponent(path))
        .then(response => {
            if (!response.ok) throw new Error("Не удалось загрузить папку");
            return response.json(); // Автоматически превращаем JSON-строку в JS-массив
        })
        .then(items => {
            renderTable(items);
        })
        .catch(err => {
            console.error("Ошибка:", err);
            let tbody = document.getElementById("fileTable");
            if (tbody) tbody.innerHTML = `<tr><td colspan="3" style="color:red;">Ошибка загрузки списка!</td></tr>`;
        });
}

// 2. Функция отрисовки таблицы HTML из полученного массива
function renderTable(items) {
    let tbody = document.getElementById("fileTable");
    if (!tbody) return;
    
    tbody.innerHTML = ""; // Полностью очищаем таблицу перед рисовкой

    // Если мы находимся НЕ в корне — первой строкой добавляем папку "назад" (..)
    if (currentDir !== "/") {
        // Отрезаем последний элемент пути, чтобы получить родительскую папку
        let lastSlash = currentDir.lastIndexOf('/');
        let parentDir = currentDir.substring(0, lastSlash);
        if (parentDir === "") parentDir = "/";

        let tr = document.createElement("tr");
        tr.innerHTML = `
            <td colspan="3">
                <a href="#" onclick="loadFolder('${parentDir}'); return false;">📁 .. (назад)</a>
            </td>`;
        tbody.appendChild(tr);
    }

    // Если папка пустая
    if (items.length === 0) {
        let tr = document.createElement("tr");
        tr.innerHTML = `<td colspan="3" style="text-align:center; color:#888;">Папка пуста</td>`;
        tbody.appendChild(tr);
        return;
    }

    // Проходимся по каждому элементу массива
    items.forEach(item => {
        let tr = document.createElement("tr");

        // Если это ПАПКА
        if (item.isDir) {
            tr.innerHTML = `
                <td>
                    📂 <a href="#" onclick="loadFolder('${item.path}'); return false;"><b>${item.name}</b></a>
                </td>
                <td>-</td>
                <td>
                    <button onclick="renameItem('${item.path}')">✏️</button>
                    <button onclick="deleteItem('${item.path}')">🗑️</button>
                </td>`;
        } 
        // Если это ФАЙЛ
        else {
            let warning = item.corrupted ? ' <span title="Шум на SPI">⚠️</span>' : '';
            tr.innerHTML = `
                <td>📄 ${item.name}${warning}</td>
                <td>${formatBytes(item.size)}</td>
                <td>
                    <button onclick="downloadFile('${item.path}')">⬇️</button>
                    <button onclick="renameItem('${item.path}')">✏️</button>
                    <button onclick="deleteItem('${item.path}')">🗑️</button>
                </td>`;
        }
        tbody.appendChild(tr);
    });
}

// 3. Скачивание файла
function downloadFile(path) {
    // Просто перенаправляем браузер по ссылке — он сам начнет скачивание
    window.location.href = '/download?file=' + encodeURIComponent(path);
}

// 4. Удаление (файла или папки)
function deleteItem(path) {
    if (!confirm(`Удалить "${path}"?`)) return;

    fetch('/delete?file=' + encodeURIComponent(path), { method: 'DELETE' })
        .then(res => {
            if (res.ok) {
                loadFolder(currentDir); // Перезагружаем текущую папку
            } else {
                alert("Ошибка при удалении!");
            }
        })
        .catch(err => alert("Ошибка сети!"));
}

// 5. Переименование
function renameItem(oldPath) {
    let newName = prompt("Введите новое имя/путь:", oldPath);
    if (!newName || newName === oldPath) return;

    let formData = new FormData();
    // Наш C++ хендл /rename ждет параметры "from" и "to"
    fetch(`/rename?from=${encodeURIComponent(oldPath)}&to=${encodeURIComponent(newName)}`, { method: 'POST' })
        .then(res => {
            if (res.ok) {
                loadFolder(currentDir); // Перезагружаем папку
            } else {
                alert("Ошибка при переименовании!");
            }
        });
}

// 6. Загрузка файла на ESP32 (с ползунком прогресса)
function uploadFile() {
    let fileInput = document.getElementById("fileInput");
    let statusDiv = document.getElementById("status1");

    if (!fileInput || fileInput.files.length === 0) {
        alert("Выберите файл!");
        return;
    }

    let file = fileInput.files[0];
    let xhr = new XMLHttpRequest();
    let formData = new FormData();

    // Формируем путь, куда загрузить (в текущую открытую папку)
    let uploadPath = currentDir;
    if (!uploadPath.endsWith("/")) uploadPath += "/";
    uploadPath += file.name;

    formData.append("data", file, uploadPath);

    // Отслеживаем прогресс загрузки
    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            let percent = Math.round((e.loaded / e.total) * 100);
            if (statusDiv) statusDiv.innerText = `Загрузка: ${percent}%`;
        }
    };

    xhr.onload = function() {
        if (xhr.status === 200) {
            if (statusDiv) statusDiv.innerText = "Загрузка завершена!";
            fileInput.value = ""; // Очищаем поле выбора
            loadFolder(currentDir); // Обновляем список файлов
        } else {
            if (statusDiv) statusDiv.innerText = "Ошибка загрузки!";
        }
    };

    xhr.open("POST", "/upload", true);
    xhr.send(formData);
}

// Хелпер: красивое форматирование размера байтов (например, 1024 B -> 1.0 KB)
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// Загрузка фолдера
function uploadFolder()
{
    let folderName = document.getElementById("folderInput");
    let statusDiv = document.getElementById("status2");

    let realName = folderName.value;

    if (!realName.trim()) 
    {
        alert("Напишите имя файла !!!");
        return;
    }

    let uploadPath = currentDir;
    if (!uploadPath.endsWith("/")) uploadPath += "/";
    uploadPath += realName;

    if (statusDiv) statusDiv.innerText = "Создание папки...";

    fetch('/mkdir?path=' + encodeURIComponent(uploadPath), { method: 'POST' })
        .then(response => {
            if (response.ok)
            {
                if (statusDiv) statusDiv.innerText = "Папка создана!";
                folderName.value = ""; 
                loadFolder(currentDir);
            }
            else
            {
                if (statusDiv) statusDiv.innerText = "Ошибка создания!";
            }
        })
        .catch(error => {
            if (statusDiv) statusDiv.innerText = "Ошибка сети!";
        });
}

// Запускаем автоматическую загрузку корня при открытии страницы
window.onload = () => {
    loadFolder("/");
};
)rawliteral";