import http.server
import socketserver
import json
import os

PORT = 8080

HTML_PAGE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LoRa Config Manager</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --primary: #3b82f6;
            --primary-hover: #2563eb;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border: #334155;
            --success: #10b981;
        }
        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-main);
            margin: 0;
            padding: 2rem;
            display: flex;
            justify-content: center;
        }
        .container {
            width: 100%;
            max-width: 600px;
        }
        .card {
            background: var(--card-bg);
            border-radius: 16px;
            padding: 2rem;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5);
            border: 1px solid var(--border);
        }
        h1 {
            margin-top: 0;
            font-size: 1.75rem;
            font-weight: 700;
            margin-bottom: 1.5rem;
            color: var(--text-main);
            text-align: center;
        }
        .form-group {
            margin-bottom: 1.25rem;
        }
        label {
            display: block;
            margin-bottom: 0.5rem;
            font-size: 0.875rem;
            color: var(--text-muted);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }
        input[type="text"], input[type="number"] {
            width: 100%;
            padding: 0.875rem;
            border-radius: 8px;
            background: var(--bg-color);
            border: 1px solid var(--border);
            color: var(--text-main);
            font-size: 1rem;
            box-sizing: border-box;
            transition: all 0.2s;
        }
        input:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.25);
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            gap: 0.75rem;
            margin-top: 1rem;
            padding: 1rem;
            background: rgba(255, 255, 255, 0.03);
            border-radius: 8px;
            border: 1px solid var(--border);
        }
        input[type="checkbox"] {
            width: 1.25rem;
            height: 1.25rem;
            accent-color: var(--primary);
            cursor: pointer;
        }
        .checkbox-group label {
            margin: 0;
            font-size: 1rem;
            text-transform: none;
            color: var(--text-main);
            cursor: pointer;
        }
        button {
            width: 100%;
            padding: 1rem;
            background: linear-gradient(135deg, var(--primary), var(--primary-hover));
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 1.125rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            margin-top: 1.5rem;
            box-shadow: 0 4px 6px -1px rgba(59, 130, 246, 0.5);
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 8px -1px rgba(59, 130, 246, 0.6);
        }
        button:active {
            transform: translateY(0);
        }
        .toast {
            position: fixed;
            bottom: 2rem;
            right: 2rem;
            background: var(--success);
            color: white;
            padding: 1rem 1.5rem;
            border-radius: 8px;
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.2);
            opacity: 0;
            transform: translateY(10px);
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            pointer-events: none;
            font-weight: 500;
        }
        .toast.show {
            opacity: 1;
            transform: translateY(0);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h1>LoRa Configuration</h1>
            <form id="configForm">
                <div class="form-group">
                    <label>Frequency (Hz)</label>
                    <input type="number" id="frequency" name="frequency">
                </div>
                <div class="form-group">
                    <label>TX Power (dBm)</label>
                    <input type="number" id="tx_power" name="tx_power">
                </div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 1rem;">
                    <div class="form-group">
                        <label>Spreading Factor</label>
                        <input type="text" id="spreading_factor" name="spreading_factor">
                    </div>
                    <div class="form-group">
                        <label>Bandwidth</label>
                        <input type="text" id="bandwidth" name="bandwidth">
                    </div>
                </div>
                <div class="form-group">
                    <label>Coding Rate</label>
                    <input type="text" id="coding_rate" name="coding_rate">
                </div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="modbus_enabled" name="modbus_enabled">
                    <label for="modbus_enabled">Enable Modbus Protocol</label>
                </div>
                <div style="display: grid; grid-template-columns: 1fr 2fr; gap: 1rem;">
                    <div class="form-group">
                        <label>Slave ID</label>
                        <input type="number" id="modbus_slave_id" name="modbus_slave_id">
                    </div>
                    <div class="form-group">
                        <label>Address Devices</label>
                        <input type="text" id="modbus_address_devices" name="modbus_address_devices" placeholder="e.g. 1, 2, 3">
                    </div>
                </div>
                <button type="submit">Save & Reload Device</button>
            </form>
        </div>
    </div>
    <div id="toast" class="toast">Configuration saved successfully!</div>

    <script>
        fetch('/api/config')
            .then(res => res.json())
            .then(data => {
                document.getElementById('frequency').value = data.frequency || 915000000;
                document.getElementById('tx_power').value = data.tx_power || 22;
                document.getElementById('spreading_factor').value = data.spreading_factor || 'SF9';
                document.getElementById('bandwidth').value = data.bandwidth || '125';
                document.getElementById('coding_rate').value = data.coding_rate || '4/6';
                document.getElementById('modbus_enabled').checked = data.modbus_enabled || false;
                document.getElementById('modbus_slave_id').value = data.modbus_slave_id || 1;
                document.getElementById('modbus_address_devices').value = (data.modbus_address_devices || []).join(', ');
            });

        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            let devicesStr = document.getElementById('modbus_address_devices').value;
            let devices = devicesStr.split(',').map(s => parseInt(s.trim())).filter(n => !isNaN(n));

            const config = {
                frequency: parseInt(document.getElementById('frequency').value),
                tx_power: parseInt(document.getElementById('tx_power').value),
                spreading_factor: document.getElementById('spreading_factor').value,
                bandwidth: document.getElementById('bandwidth').value,
                coding_rate: document.getElementById('coding_rate').value,
                preamble_length: 8,
                rx_timeout: 5000,
                modbus_enabled: document.getElementById('modbus_enabled').checked,
                modbus_slave_id: parseInt(document.getElementById('modbus_slave_id').value),
                modbus_address_devices: devices
            };

            fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(config)
            }).then(() => {
                const toast = document.getElementById('toast');
                toast.classList.add('show');
                setTimeout(() => toast.classList.remove('show'), 3000);
            });
        });
    </script>
</body>
</html>
"""

class ConfigHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode('utf-8'))
        elif self.path == '/api/config':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            try:
                with open('config.json', 'rb') as f:
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.wfile.write(b'{}')
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if self.path == '/api/config':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            with open('config.json', 'wb') as f:
                f.write(post_data)
                
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"status": "ok"}')

with socketserver.TCPServer(("", PORT), ConfigHandler) as httpd:
    print(f"Serving LoRa Config Web App at http://localhost:{PORT}")
    httpd.serve_forever()
