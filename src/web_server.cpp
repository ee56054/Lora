#include "web_server.h"
#include <httplib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

static const char* HTML_PAGE = R"RAW(
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
                    <input type="number" id="frequency" name="frequency" value="915000000">
                </div>
                <div class="form-group">
                    <label>TX Power (dBm)</label>
                    <input type="number" id="tx_power" name="tx_power" min="-9" max="22" step="1" value="22">
                </div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 1rem;">
                    <div class="form-group">
                        <label>Spreading Factor</label>
                        <select id="spreading_factor" name="spreading_factor">
                            <option value="SF5">SF5</option>
                            <option value="SF6">SF6</option>
                            <option value="SF7">SF7</option>
                            <option value="SF8">SF8</option>
                            <option value="SF9" selected>SF9</option>
                            <option value="SF10">SF10</option>
                            <option value="SF11">SF11</option>
                            <option value="SF12">SF12</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>Bandwidth</label>
                        <select id="bandwidth" name="bandwidth">
                            <option value="7.81">7.81</option>
                            <option value="10.42">10.42</option>
                            <option value="15.63">15.63</option>
                            <option value="20.83">20.83</option>
                            <option value="31.25">31.25</option>
                            <option value="41.67">41.67</option>
                            <option value="62.5">62.5</option>
                            <option value="125" selected>125</option>
                            <option value="250">250</option>
                            <option value="500">500</option>
                        </select>
                    </div>
                </div>
                <div class="form-group">
                    <label>Coding Rate</label>
                    <select id="coding_rate" name="coding_rate">
                        <option value="4/5">4/5</option>
                        <option value="4/6" selected>4/6</option>
                        <option value="4/7">4/7</option>
                        <option value="4/8">4/8</option>
                    </select>
                </div>
                <div class="form-group checkbox-group">
                    <input type="checkbox" id="modbus_enabled" name="modbus_enabled">
                    <input type="checkbox" id="modbus_enabled" name="modbus_enabled" checked>
                    <label for="modbus_enabled">Enable Modbus Protocol</label>
                </div>
                <div style="display: grid; grid-template-columns: 1fr 2fr; gap: 1rem;">
                    <div class="form-group">
                        <label>Slave ID</label>
                        <input type="number" id="modbus_slave_id" name="modbus_slave_id">
                        <input type="number" id="modbus_slave_id" name="modbus_slave_id" value="1">
                    </div>
                    <div class="form-group">
                        <label>Address Devices</label>
                        <input type="text" id="modbus_address_devices" name="modbus_address_devices" placeholder="e.g. 1, 2, 3">
                        <input type="text" id="modbus_address_devices" name="modbus_address_devices" value="1" placeholder="e.g. 1, 2, 3">
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
                document.getElementById('modbus_enabled').checked = (data.modbus_enabled !== undefined) ? data.modbus_enabled : true;
                document.getElementById('modbus_slave_id').value = data.modbus_slave_id || 1;
                document.getElementById('modbus_address_devices').value = (data.modbus_address_devices || []).join(', ');
                document.getElementById('modbus_address_devices').value = (data.modbus_address_devices && data.modbus_address_devices.length > 0) ? data.modbus_address_devices.join(', ') : '1';
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
                body: JSON.stringify(config, null, 4)
            }).then(() => {
                const toast = document.getElementById('toast');
                toast.classList.add('show');
                setTimeout(() => toast.classList.remove('show'), 3000);
            });
        });
    </script>
</body>
</html>
)RAW";

void start_web_server() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(HTML_PAGE, "text/html");
    });

    svr.Get("/api/config", [](const httplib::Request& req, httplib::Response& res) {
        std::ifstream file("config.json");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "application/json");
        } else {
            res.set_content("{}", "application/json");
        }
    });

    svr.Post("/api/config", [](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json j = nlohmann::json::parse(req.body);
            std::ofstream file("config.json");
            if (file.is_open()) {
                file << j.dump(4) << std::endl;
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\": \"Could not write to file\"}", "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid JSON\"}", "application/json");
        }
    });

    std::cout << "\n>>> Starting Embedded C++ Web Server on http://0.0.0.0:8080 <<<\n" << std::endl;
    svr.listen("0.0.0.0", 8080);
}
