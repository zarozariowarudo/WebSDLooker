#pragma once

inline const char* cssStyle = 
R"rawliteral(
    * { box-sizing: border-box; }
    body 
    { 
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
        background: #121212; color: #e0e0e0; 
        margin: 0; 
        padding: 20px; 
    }

    .container 
    { 
        max-width: 900px; 
        margin: auto; 
        background: #1e1e1e; 
        padding: 25px; 
        border-radius: 12px; 
        box-shadow: 0 8px 24px rgba(0,0,0,0.6); 
    }

    .upload-box
    {
        display: flex;
        flex-direction: row;
        gap: 10px;
        align-items: flex-start;
        background: #2a2a2a; 
        padding: 20px; 
        border-radius: 8px; 
        margin-bottom: 25px; 
        border: 1px solid #3d3d3d; 
    }

    .FileUploadBox
    {
        flex-basis: 50%;
        display: flex;
        flex-direction: column;
        align-items: flex-start;
        gap: 5px;
    }

    .FolderUploadBox
    {
        flex-basis: 50%;
        display: flex;
        flex-direction: column; 
        align-items: flex-start;
        gap: 5px;
    }

    .FolderUploadBox a
    {
        display: inline-block;
        max-width: 30px;
    }

    h2 
    { 
        text-align: center; 
        color: #4CAF50; 
        margin-top: 0; 
    }

    input[type="file"] 
    { 
        margin-bottom: 12px; 
        color: #ccc; 
    }
        
    input[type="text"]
    {
        margin-bottom: 12px;
    }

    button 
    { 
        background: #4CAF50; 
        color: white; 
        border: none; 
        padding: 9px 15px; 
        cursor: pointer; 
        border-radius: 6px; 
        font-weight: bold; 
        transition: 0.2s; 
        margin-right: 5px; 
    }
    
    button:hover { background: #45a049; }
    .btn-danger { background: #f44336; }
    .btn-danger:hover { background: #d32f2f; }
    .btn-rename { background: #2196F3; }
    .btn-rename:hover { background: #0b7dda; }

    table 
    { 
        width: 100%; 
        border-collapse: 
        collapse; 
        margin-top: 10px; 
    }

    th, td 
    { 
        padding: 12px; 
        border-bottom: 1px solid #333; 
        text-align: left; 
    }
    th 
    { 
        background: #282828; 
        color: #aaa; 
    }

    tr:hover { background: #252525; }
    
    a 
    { 
        color: #81c784; 
        text-decoration: none; 
        word-break: break-all; 
    }

    a:hover { text-decoration: underline; }

    #status1 { font-weight: bold; margin-top: 10px; }
    #status2 { font-weight: bold; margin-top: 10px; }
)rawliteral";