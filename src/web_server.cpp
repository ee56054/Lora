#include "web_server.h"
#include "modbus_app.h"
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
    <title>LoRa & Modbus Dashboard</title>
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
            --danger: #ef4444;
        }
        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-main);
            margin: 0;
            padding: 2rem 1rem;
            display: flex;
            justify-content: center;
        }
        .container {
            width: 100%;
            max-width: 820px;
        }
        .card {
            background: var(--card-bg);
            border-radius: 16px;
            padding: 2rem;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5);
            border: 1px solid var(--border);
            margin-bottom: 1.5rem;
        }
        h1 {
            margin-top: 0;
            font-size: 1.6rem;
            font-weight: 700;
            margin-bottom: 1.5rem;
            color: var(--text-main);
        }
        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1.25rem;
            flex-wrap: wrap;
            gap: 0.75rem;
        }
        .card-header h1 {
            margin-bottom: 0;
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
        input[type="text"], input[type="number"], select {
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
        input:focus, select:focus {
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
            padding: 0.9rem;
            background: linear-gradient(135deg, var(--primary), var(--primary-hover));
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
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
        .btn-sm {
            padding: 0.4rem 0.75rem;
            font-size: 0.8rem;
            border-radius: 6px;
            width: auto;
            margin: 0;
            background: rgba(255, 255, 255, 0.08);
            border: 1px solid var(--border);
            color: var(--text-main);
            box-shadow: none;
        }
        .btn-sm:hover {
            background: var(--primary);
            color: white;
            border-color: var(--primary);
            transform: translateY(-1px);
        }
        .btn-success {
            background: linear-gradient(135deg, #10b981, #059669);
            box-shadow: 0 4px 6px -1px rgba(16, 185, 129, 0.4);
        }
        .btn-success:hover {
            box-shadow: 0 6px 8px -1px rgba(16, 185, 129, 0.5);
        }
        .telem-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.9rem;
            margin-top: 0.5rem;
        }
        .telem-table th, .telem-table td {
            padding: 0.7rem 0.85rem;
            text-align: left;
            border-bottom: 1px solid var(--border);
        }
        .telem-table th {
            color: var(--text-muted);
            font-weight: 600;
            font-size: 0.8rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }
        .table-section td {
            background: rgba(255, 255, 255, 0.04);
            font-weight: 700;
            font-size: 0.8rem;
            color: var(--primary);
            text-transform: uppercase;
            letter-spacing: 0.05em;
            padding: 0.5rem 0.85rem;
        }
        .badge {
            display: inline-block;
            padding: 0.25rem 0.65rem;
            border-radius: 6px;
            font-size: 0.825rem;
            font-weight: 600;
        }
        .badge-open {
            background: #065f46;
            color: #34d399;
        }
        .badge-closed {
            background: #374151;
            color: #9ca3af;
        }
        .badge-val {
            background: #1e3a8a;
            color: #93c5fd;
            font-family: monospace;
        }
        .badge-unknown {
            background: rgba(255, 255, 255, 0.1);
            color: var(--text-muted);
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
            z-index: 9999;
        }
        .toast.show {
            opacity: 1;
            transform: translateY(0);
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Card 1: Modbus Live Telemetry & Reads -->
        <div class="card">
            <div class="card-header">
                <h1>Modbus Live Telemetry</h1>
                <button type="button" id="btnReadAll" class="btn-success" style="margin: 0; width: auto; padding: 0.65rem 1.25rem; font-size: 0.95rem;" onclick="readAllTelemetry()">
                    &#x21bb; Read All Values
                </button>
            </div>
            
            <div style="font-size: 0.85rem; color: var(--text-muted); margin-bottom: 1.25rem; display: flex; justify-content: space-between; flex-wrap: wrap; gap: 0.5rem; background: rgba(255,255,255,0.02); padding: 0.75rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                <span>Target Slave ID: <strong id="telem_slave_id_disp" style="color: var(--primary);">1</strong></span>
                <span>Last Updated: <strong id="last_update_disp" style="color: var(--text-main);">Never</strong></span>
            </div>

            <div style="overflow-x: auto;">
                <table class="telem-table">
                    <thead>
                        <tr>
                            <th>Type & Address</th>
                            <th>Parameter Name</th>
                            <th>Value</th>
                            <th style="text-align: right;">Action</th>
                        </tr>
                    </thead>
                    <tbody>
                        <!-- Coils (0x) -->
                        <tr class="table-section"><td colspan="4">Coils (0x / Read-Write)</td></tr>
                        <tr>
                            <td><code>0x0000 (0)</code></td>
                            <td>Valve 1</td>
                            <td id="val_valve1"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('coil', 0, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0001 (1)</code></td>
                            <td>Valve 2</td>
                            <td id="val_valve2"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('coil', 1, this)">Read</button></td>
                        </tr>

                        <!-- Discrete Inputs (1x) -->
                        <tr class="table-section"><td colspan="4">Discrete Inputs (1x / Read-Only)</td></tr>
                        <tr>
                            <td><code>0x0000 (0)</code></td>
                            <td>Valve 1 Status</td>
                            <td id="val_valve1_status"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('discrete_input', 0, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0001 (1)</code></td>
                            <td>Valve 2 Status</td>
                            <td id="val_valve2_status"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('discrete_input', 1, this)">Read</button></td>
                        </tr>

                        <!-- Input Registers (3x) -->
                        <tr class="table-section"><td colspan="4">Input Registers (3x / Read-Only)</td></tr>
                        <tr>
                            <td><code>0x0000 (0)</code></td>
                            <td>Sensor 1 Reading</td>
                            <td id="val_sensor1"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('input_register', 0, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0001 (1)</code></td>
                            <td>Sensor 2 Reading</td>
                            <td id="val_sensor2"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('input_register', 1, this)">Read</button></td>
                        </tr>

                        <!-- Holding Registers (4x) -->
                        <tr class="table-section"><td colspan="4">Holding Registers (4x / Read-Write)</td></tr>
                        <tr>
                            <td><code>0x0000 (0)</code></td>
                            <td>Hardware ID (High)</td>
                            <td id="val_hw_id_high"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 0, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0001 (1)</code></td>
                            <td>Hardware ID (Low)</td>
                            <td id="val_hw_id_low"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 1, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0002 (2)</code></td>
                            <td>TX Counter (High)</td>
                            <td id="val_tx_count_high"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 2, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0003 (3)</code></td>
                            <td>TX Counter (Low)</td>
                            <td id="val_tx_count_low"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 3, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0004 (4)</code></td>
                            <td>Valve 1 Mirror Reg</td>
                            <td id="val_valve1_reg"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 4, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0005 (5)</code></td>
                            <td>Valve 2 Mirror Reg</td>
                            <td id="val_valve2_reg"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 5, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0006 (6)</code></td>
                            <td>Sensor 1 Mirror Reg</td>
                            <td id="val_sensor1_reg"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 6, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0007 (7)</code></td>
                            <td>Sensor 2 Mirror Reg</td>
                            <td id="val_sensor2_reg"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 7, this)">Read</button></td>
                        </tr>
                        <tr>
                            <td><code>0x0008 (8)</code></td>
                            <td>Slave ID Reg</td>
                            <td id="val_slave_id_reg"><span class="badge badge-unknown">Unknown</span></td>
                            <td style="text-align: right;"><button class="btn-sm" onclick="readItem('holding_register', 8, this)">Read</button></td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>

        <!-- Card 2: Modbus Controls (Write) -->
        <div class="card">
            <h1>Modbus Controls (Write)</h1>
            
            <div class="form-group">
                <label>Target Slave ID</label>
                <input type="number" id="ctrl_slave_id" value="1" min="1" max="247" onchange="document.getElementById('telem_slave_id_disp').innerText = this.value">
            </div>

            <label style="margin-top: 1.25rem;">Valve Control (Coils 0 & 1 via FC 05)</label>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; margin-bottom: 1.5rem;">
                <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border);">
                    <div style="font-weight: 600; margin-bottom: 0.5rem; font-size: 0.9rem;">Valve 1 (Coil 0x0)</div>
                    <div style="display: flex; gap: 0.5rem;">
                        <button type="button" style="margin-top: 0; background: #10b981; padding: 0.6rem;" onclick="setValve(1, true)">Open (1)</button>
                        <button type="button" style="margin-top: 0; background: #ef4444; padding: 0.6rem;" onclick="setValve(1, false)">Close (0)</button>
                    </div>
                </div>
                <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border);">
                    <div style="font-weight: 600; margin-bottom: 0.5rem; font-size: 0.9rem;">Valve 2 (Coil 0x1)</div>
                    <div style="display: flex; gap: 0.5rem;">
                        <button type="button" style="margin-top: 0; background: #10b981; padding: 0.6rem;" onclick="setValve(2, true)">Open (1)</button>
                        <button type="button" style="margin-top: 0; background: #ef4444; padding: 0.6rem;" onclick="setValve(2, false)">Close (0)</button>
                    </div>
                </div>
            </div>

            <label>Write Holding Register (FC 06)</label>
            <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border); display: grid; grid-template-columns: 1fr 1fr auto; gap: 0.75rem; align-items: end;">
                <div class="form-group" style="margin: 0;">
                    <label style="font-size: 0.75rem;">Register Addr</label>
                    <input type="number" id="write_reg_addr" value="8" placeholder="e.g. 8 for Slave ID">
                </div>
                <div class="form-group" style="margin: 0;">
                    <label style="font-size: 0.75rem;">Value (16-bit)</label>
                    <input type="number" id="write_reg_val" value="1" placeholder="Value">
                </div>
                <button type="button" style="margin-top: 0; padding: 0.85rem 1.25rem; width: auto;" onclick="writeRegister()">Send Write</button>
            </div>
        </div>

        <!-- Card 3: LoRa Hardware Configuration -->
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
                    <input type="checkbox" id="modbus_enabled" name="modbus_enabled" checked>
                    <label for="modbus_enabled">Enable Modbus Protocol</label>
                </div>
                <div style="display: grid; grid-template-columns: 1fr 2fr; gap: 1rem;">
                    <div class="form-group">
                        <label>Local Slave ID</label>
                        <input type="number" id="modbus_slave_id" name="modbus_slave_id" value="1">
                    </div>
                    <div class="form-group">
                        <label>Address Devices</label>
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
                document.getElementById('modbus_enabled').checked = (data.modbus_enabled !== undefined) ? data.modbus_enabled : true;
                document.getElementById('modbus_slave_id').value = data.modbus_slave_id || 1;
                document.getElementById('ctrl_slave_id').value = data.modbus_slave_id || 1;
                document.getElementById('telem_slave_id_disp').innerText = data.modbus_slave_id || 1;
                document.getElementById('modbus_address_devices').value = (data.modbus_address_devices && data.modbus_address_devices.length > 0) ? data.modbus_address_devices.join(', ') : '1';
            });

        function showToast(msg, isSuccess = true) {
            const toast = document.getElementById('toast');
            toast.innerText = msg;
            toast.style.background = isSuccess ? 'var(--success)' : 'var(--danger)';
            toast.classList.add('show');
            setTimeout(() => toast.classList.remove('show'), 3000);
        }

        function updateTelemetryUI(data) {
            if (!data) return;
            if (data.slave_id) {
                document.getElementById('telem_slave_id_disp').innerText = data.slave_id;
            }
            document.getElementById('last_update_disp').innerText = data.last_update || 'Never';

            const renderBool = (val, labelTrue = 'OPEN (1)', labelFalse = 'CLOSED (0)') => {
                if (val === 1 || val === true) return `<span class="badge badge-open">${labelTrue}</span>`;
                if (val === 0 || val === false) return `<span class="badge badge-closed">${labelFalse}</span>`;
                return `<span class="badge badge-unknown">Unknown</span>`;
            };

            const renderNum = (val, suffix = '', hex = false) => {
                if (val === undefined || val === null || val < 0) return `<span class="badge badge-unknown">Unknown</span>`;
                let str = val.toString();
                if (hex) str += ` (0x${val.toString(16).toUpperCase()})`;
                if (suffix) str += ` ${suffix}`;
                return `<span class="badge badge-val">${str}</span>`;
            };

            document.getElementById('val_valve1').innerHTML = renderBool(data.valve1);
            document.getElementById('val_valve2').innerHTML = renderBool(data.valve2);
            document.getElementById('val_valve1_status').innerHTML = renderBool(data.valve1_status, 'OPEN (1)', 'CLOSED (0)');
            document.getElementById('val_valve2_status').innerHTML = renderBool(data.valve2_status, 'OPEN (1)', 'CLOSED (0)');
            
            // Sensors (e.g. sensor1 250 -> 25.0 °C, sensor2 1013 -> 1013 hPa)
            let s1_str = (data.sensor1 >= 0) ? `${data.sensor1} (${(data.sensor1 / 10.0).toFixed(1)} °C)` : null;
            let s2_str = (data.sensor2 >= 0) ? `${data.sensor2} (${data.sensor2} hPa)` : null;
            document.getElementById('val_sensor1').innerHTML = s1_str ? `<span class="badge badge-val">${s1_str}</span>` : `<span class="badge badge-unknown">Unknown</span>`;
            document.getElementById('val_sensor2').innerHTML = s2_str ? `<span class="badge badge-val">${s2_str}</span>` : `<span class="badge badge-unknown">Unknown</span>`;

            document.getElementById('val_hw_id_high').innerHTML = renderNum(data.hw_id_high, '', true);
            document.getElementById('val_hw_id_low').innerHTML = renderNum(data.hw_id_low, '', true);
            document.getElementById('val_tx_count_high').innerHTML = renderNum(data.tx_count_high);
            document.getElementById('val_tx_count_low').innerHTML = renderNum(data.tx_count_low);
            document.getElementById('val_valve1_reg').innerHTML = renderNum(data.valve1_reg);
            document.getElementById('val_valve2_reg').innerHTML = renderNum(data.valve2_reg);
            document.getElementById('val_sensor1_reg').innerHTML = renderNum(data.sensor1_reg);
            document.getElementById('val_sensor2_reg').innerHTML = renderNum(data.sensor2_reg);
            document.getElementById('val_slave_id_reg').innerHTML = renderNum(data.slave_id_reg);
        }

        function pollTelemetry() {
            fetch('/api/modbus/telemetry')
                .then(res => res.json())
                .then(data => updateTelemetryUI(data))
                .catch(err => console.debug("Telemetry fetch idle", err));
        }

        function readItem(type, address, btn) {
            const slaveId = parseInt(document.getElementById('ctrl_slave_id').value) || 1;
            const originalText = btn.innerText;
            btn.innerText = '...';
            btn.disabled = true;

            fetch('/api/modbus/read', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, type: type, address: address })
            }).then(res => res.json()).then(data => {
                btn.innerText = originalText;
                btn.disabled = false;
                if (data.status === 'ok') {
                    showToast(`Read ${type} [${address}] = ${data.value}`);
                    pollTelemetry();
                } else {
                    showToast(`Read error: ${data.error}`, false);
                }
            }).catch(err => {
                btn.innerText = originalText;
                btn.disabled = false;
                showToast(`Read request failed`, false);
            });
        }

        function readAllTelemetry() {
            const slaveId = parseInt(document.getElementById('ctrl_slave_id').value) || 1;
            const btn = document.getElementById('btnReadAll');
            const origText = btn.innerHTML;
            btn.innerHTML = 'Reading all...';
            btn.disabled = true;

            fetch('/api/modbus/read_all', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId })
            }).then(res => res.json()).then(data => {
                btn.innerHTML = origText;
                btn.disabled = false;
                if (data.status === 'ok') {
                    showToast(`All telemetry refreshed for Slave ${slaveId}`);
                    if (data.telemetry) updateTelemetryUI(data.telemetry);
                    else pollTelemetry();
                } else {
                    showToast(`Read all error: ${data.error}`, false);
                }
            }).catch(err => {
                btn.innerHTML = origText;
                btn.disabled = false;
                showToast(`Read all request failed`, false);
            });
        }

        function setValve(valveIndex, open) {
            const slaveId = parseInt(document.getElementById('ctrl_slave_id').value) || 1;
            fetch('/api/modbus/write_valve', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, valve: valveIndex, open: open })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`Valve ${valveIndex} set to ${open ? 'OPEN (1)' : 'CLOSED (0)'}`);
                    pollTelemetry();
                } else {
                    showToast(`Write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

        function writeRegister() {
            const slaveId = parseInt(document.getElementById('ctrl_slave_id').value) || 1;
            const addr = parseInt(document.getElementById('write_reg_addr').value);
            const val = parseInt(document.getElementById('write_reg_val').value);
            fetch('/api/modbus/write_register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, address: addr, value: val })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`Register ${addr} written with ${val}`);
                    pollTelemetry();
                } else {
                    showToast(`Write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

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
                showToast('Configuration saved successfully!');
            });
        });

        // Initialize telemetry polling every 2.5s
        setInterval(pollTelemetry, 2500);
        pollTelemetry();
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

    svr.Get("/api/modbus/telemetry", [](const httplib::Request& req, httplib::Response& res) {
        ModbusTelemetry telem = get_modbus_telemetry();
        nlohmann::json j;
        j["slave_id"] = telem.slave_id;
        j["valve1"] = telem.valve1;
        j["valve2"] = telem.valve2;
        j["valve1_status"] = telem.valve1_status;
        j["valve2_status"] = telem.valve2_status;
        j["sensor1"] = telem.sensor1;
        j["sensor2"] = telem.sensor2;
        j["hw_id_high"] = telem.hw_id_high;
        j["hw_id_low"] = telem.hw_id_low;
        j["tx_count_high"] = telem.tx_count_high;
        j["tx_count_low"] = telem.tx_count_low;
        j["valve1_reg"] = telem.valve1_reg;
        j["valve2_reg"] = telem.valve2_reg;
        j["sensor1_reg"] = telem.sensor1_reg;
        j["sensor2_reg"] = telem.sensor2_reg;
        j["slave_id_reg"] = telem.slave_id_reg;
        j["last_update"] = telem.last_update;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/modbus/read", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            std::string type = j.value("type", "");
            int address = j.value("address", 0);
            
            bool success = false;
            nlohmann::json resp;

            if (type == "coil") {
                bool val = false;
                success = modbus_read_coil_val(slave_id, address, val);
                resp["value"] = val ? 1 : 0;
            } else if (type == "discrete_input") {
                bool val = false;
                success = modbus_read_discrete_input_val(slave_id, address, val);
                resp["value"] = val ? 1 : 0;
            } else if (type == "input_register") {
                uint16_t val = 0;
                success = modbus_read_input_reg_val(slave_id, address, val);
                resp["value"] = val;
            } else if (type == "holding_register") {
                uint16_t val = 0;
                success = modbus_read_holding_reg_val(slave_id, address, val);
                resp["value"] = val;
            } else {
                res.status = 400;
                res.set_content("{\"status\": \"error\", \"error\": \"Invalid register type\"}", "application/json");
                return;
            }

            if (success) {
                resp["status"] = "ok";
                res.set_content(resp.dump(), "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to read Modbus data";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/read_all", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            bool success = modbus_read_all(slave_id);
            if (success) {
                ModbusTelemetry telem = get_modbus_telemetry();
                nlohmann::json resp;
                resp["status"] = "ok";
                resp["telemetry"] = {
                    {"slave_id", telem.slave_id},
                    {"valve1", telem.valve1},
                    {"valve2", telem.valve2},
                    {"valve1_status", telem.valve1_status},
                    {"valve2_status", telem.valve2_status},
                    {"sensor1", telem.sensor1},
                    {"sensor2", telem.sensor2},
                    {"hw_id_high", telem.hw_id_high},
                    {"hw_id_low", telem.hw_id_low},
                    {"tx_count_high", telem.tx_count_high},
                    {"tx_count_low", telem.tx_count_low},
                    {"valve1_reg", telem.valve1_reg},
                    {"valve2_reg", telem.valve2_reg},
                    {"sensor1_reg", telem.sensor1_reg},
                    {"sensor2_reg", telem.sensor2_reg},
                    {"slave_id_reg", telem.slave_id_reg},
                    {"last_update", telem.last_update}
                };
                res.set_content(resp.dump(), "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to read all values";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/write_coil", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int address = j.value("address", 0);
            bool value = j.value("value", false);
            bool success = modbus_write_coil(slave_id, address, value);
            if (success) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to write coil";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/write_register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int address = j.value("address", 0);
            uint16_t value = j.value("value", 0);
            bool success = modbus_write_holding_register(slave_id, address, value);
            if (success) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to write register";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/write_valve", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int valve_index = j.value("valve", 1);
            bool open = j.value("open", false);
            bool success = modbus_write_valve(slave_id, valve_index, open);
            if (success) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to write valve";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    std::cout << "\n>>> Starting Embedded C++ Web Server on http://0.0.0.0:8080 <<<\n" << std::endl;
    svr.listen("0.0.0.0", 8080);
}

