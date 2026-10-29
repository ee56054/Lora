#include "web_server.h"
#include "config.h"
#include "modbus_app.h"
#include "mqtt_bridge.h"
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
        input[type="text"], input[type="number"], input[type="password"], select {
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
        /* Multi-Tab Navigation */
        .app-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 1rem;
            margin-bottom: 1.25rem;
        }
        .app-title {
            font-size: 1.5rem;
            font-weight: 800;
            margin: 0;
            background: linear-gradient(135deg, #60a5fa, #34d399);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .tabs-nav {
            display: flex;
            gap: 0.5rem;
            background: var(--card-bg);
            padding: 0.45rem;
            border-radius: 12px;
            border: 1px solid var(--border);
            margin-bottom: 1.25rem;
            overflow-x: auto;
        }
        .tab-btn {
            flex: 1;
            min-width: 140px;
            margin: 0;
            padding: 0.75rem 1rem;
            border-radius: 8px;
            background: transparent;
            color: var(--text-muted);
            font-size: 0.92rem;
            font-weight: 600;
            border: 1px solid transparent;
            box-shadow: none;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.5rem;
            transition: all 0.2s ease;
        }
        .tab-btn:hover {
            color: var(--text-main);
            background: rgba(255, 255, 255, 0.05);
            transform: none;
            box-shadow: none;
        }
        .tab-btn.active {
            background: linear-gradient(135deg, var(--primary), var(--primary-hover));
            color: #ffffff;
            box-shadow: 0 4px 10px -2px rgba(59, 130, 246, 0.5);
        }
        .tab-pane {
            display: none;
            animation: fadeIn 0.2s ease;
        }
        .tab-pane.active {
            display: block;
        }
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(4px); }
            to { opacity: 1; transform: translateY(0); }
        }
        .slave-bar {
            background: var(--card-bg);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 1rem 1.25rem;
            margin-bottom: 1.25rem;
        }
        .slave-pills {
            display: flex;
            gap: 0.4rem;
            flex-wrap: wrap;
        }
        .pill-btn {
            margin: 0;
            width: auto;
            padding: 0.45rem 0.85rem;
            font-size: 0.8rem;
            border-radius: 999px;
            background: rgba(255, 255, 255, 0.06);
            border: 1px solid var(--border);
            color: var(--text-main);
            box-shadow: none;
        }
        .pill-btn:hover {
            background: var(--primary);
            border-color: var(--primary);
            transform: translateY(-1px);
        }
        .pill-btn.active-pill {
            background: var(--primary);
            border-color: var(--primary);
            color: #fff;
        }
        .pill-btn.broadcast-pill {
            border-color: #f59e0b;
            color: #fbbf24;
            background: rgba(245, 158, 11, 0.12);
        }
        .pill-btn.broadcast-pill.active-pill,
        .pill-btn.broadcast-pill:hover {
            background: #f59e0b;
            color: #0f172a;
            font-weight: 700;
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- App Header -->
        <div class="app-header">
            <h1 class="app-title">LoRa SX126X &amp; Modbus Manager</h1>
            <div style="font-size: 0.82rem; color: var(--text-muted); background: var(--card-bg); padding: 0.45rem 0.85rem; border-radius: 999px; border: 1px solid var(--border);">
                Last Sync: <strong id="last_update_disp" style="color: var(--text-main);">Never</strong>
            </div>
        </div>

        <!-- Multi-Tab Navigation Bar -->
        <div class="tabs-nav" role="tablist">
            <button type="button" class="tab-btn active" id="tabBtn-telemetry" onclick="switchTab('telemetry')">
                &#x1F4CA; Live Telemetry
            </button>
            <button type="button" class="tab-btn" id="tabBtn-controls" onclick="switchTab('controls')">
                &#x1F39B; Modbus Controls
            </button>
            <button type="button" class="tab-btn" id="tabBtn-scan" onclick="switchTab('scan')">
                &#x1F50D; Scan Devices
            </button>
            <button type="button" class="tab-btn" id="tabBtn-mqtt" onclick="switchTab('mqtt')">
                &#x1F4E1; MQTT Bridge <span id="mqtt_tab_dot" style="display:inline-block;width:8px;height:8px;border-radius:50%;background:#9ca3af;"></span>
            </button>
            <button type="button" class="tab-btn" id="tabBtn-config" onclick="switchTab('config')">
                &#x2699; LoRa &amp; Security Config
            </button>
        </div>

        <!-- Shared Target Slave Selector Bar (Visible on Telemetry & Controls Tabs) -->
        <div id="shared_slave_bar" class="slave-bar">
            <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 0.75rem;">
                <div style="display: flex; align-items: center; gap: 0.65rem; flex-wrap: wrap;">
                    <label style="margin: 0; font-size: 0.8rem;">Target Slave ID:</label>
                    <input type="number" id="ctrl_slave_id" value="1" min="0" max="247" style="width: 90px; padding: 0.45rem 0.65rem; font-size: 0.95rem;" oninput="onTargetSlaveChanged(this.value)">
                    <span style="font-size: 0.82rem; color: var(--text-muted);">Active: <strong id="telem_slave_id_disp" style="color: var(--primary);">1</strong></span>
                </div>
                <div id="slave_pills_container" class="slave-pills">
                    <button type="button" class="pill-btn broadcast-pill" data-slave="0" onclick="setTargetSlaveId(0)">Broadcast (0)</button>
                    <button type="button" class="pill-btn active-pill" data-slave="1" onclick="setTargetSlaveId(1)">Slave 1</button>
                </div>
            </div>
            <div id="broadcast_notice" style="display: none; margin-top: 0.75rem; padding: 0.5rem 0.75rem; border-radius: 6px; background: rgba(245, 158, 11, 0.15); border: 1px solid #f59e0b; color: #fbbf24; font-size: 0.82rem;">
                <strong>Global Broadcast Mode (Address 0):</strong> Write commands will be executed by <strong>ALL</strong> slaves simultaneously without sending a reply. Read operations require a unicast Slave ID (1–247).
            </div>
        </div>

        <!-- TAB 1: Modbus Live Telemetry & Reads -->
        <div id="tab-telemetry" class="tab-pane active">
            <div class="card">
                <div class="card-header">
                    <h1>Modbus Live Telemetry</h1>
                    <button type="button" id="btnReadAll" class="btn-success" style="margin: 0; width: auto; padding: 0.65rem 1.25rem; font-size: 0.95rem;" onclick="readAllTelemetry()">
                        &#x21bb; Read All Values
                    </button>
                </div>

                <div style="overflow-x: auto;">
                    <table class="telem-table">
                        <thead>
                            <tr>
                                <th>Type &amp; Address</th>
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
        </div>

        <!-- TAB 2: Modbus Controls (Write) -->
        <div id="tab-controls" class="tab-pane">
            <div class="card">
                <h1>Modbus Controls (Write)</h1>

                <label>Valve Control (Coils 0 &amp; 1 via FC 05 / FC 15)</label>
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 1rem; margin-bottom: 1.5rem;">
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
                    <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border);">
                        <div style="font-weight: 600; margin-bottom: 0.5rem; font-size: 0.9rem;">Both Valves (FC 15 Multi-Coil)</div>
                        <div style="display: flex; gap: 0.5rem;">
                            <button type="button" style="margin-top: 0; background: #10b981; padding: 0.6rem;" onclick="setMultipleValves(true, true)">Open Both</button>
                            <button type="button" style="margin-top: 0; background: #ef4444; padding: 0.6rem;" onclick="setMultipleValves(false, false)">Close Both</button>
                        </div>
                    </div>
                </div>

                <label>Write Single Holding Register (FC 06)</label>
                <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border); display: grid; grid-template-columns: 1fr 1fr auto; gap: 0.75rem; align-items: end; margin-bottom: 1.5rem;">
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

                <label>Write Multiple Holding Registers (FC 16)</label>
                <div style="background: rgba(255,255,255,0.03); padding: 1rem; border-radius: 8px; border: 1px solid var(--border); display: grid; grid-template-columns: 1fr 2fr auto; gap: 0.75rem; align-items: end;">
                    <div class="form-group" style="margin: 0;">
                        <label style="font-size: 0.75rem;">Start Addr</label>
                        <input type="number" id="write_multi_reg_addr" value="4" placeholder="Start Addr">
                    </div>
                    <div class="form-group" style="margin: 0;">
                        <label style="font-size: 0.75rem;">Values (comma-separated)</label>
                        <input type="text" id="write_multi_reg_vals" value="1, 1" placeholder="e.g. 1, 1">
                    </div>
                    <button type="button" style="margin-top: 0; padding: 0.85rem 1.25rem; width: auto;" onclick="writeMultipleRegisters()">Send Multi-Write</button>
                </div>
            </div>
        </div>

        <!-- TAB 3: LoRa Hardware & Security Configuration -->
        <div id="tab-config" class="tab-pane">
            <div class="card">
                <h1>LoRa &amp; Security Configuration</h1>
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
                            <label>Default Slave ID</label>
                            <input type="number" id="modbus_slave_id" name="modbus_slave_id" value="1">
                        </div>
                        <div class="form-group">
                            <label>Address Devices</label>
                            <input type="text" id="modbus_address_devices" name="modbus_address_devices" value="1" placeholder="e.g. 1, 2, 3">
                        </div>
                    </div>
                    <div class="form-group checkbox-group" style="margin-top: 1.25rem;">
                        <input type="checkbox" id="security_enabled" name="security_enabled" checked>
                        <label for="security_enabled">Enable LoRa Security (AES-128-CTR + MIC, STM32 Compatible)</label>
                    </div>
                    <div class="form-group">
                        <label>AES-128 Pre-Shared Key (32 Hex Characters)</label>
                        <input type="text" id="aes_key" name="aes_key" value="2B7E151628AED2A6ABF7158809CF4F3C" maxlength="32" placeholder="e.g. 2B7E151628AED2A6ABF7158809CF4F3C" style="font-family: monospace; letter-spacing: 1px;">
                    </div>
                    <button type="submit">Save &amp; Reload Device</button>
                </form>
            </div>
        </div>

        <!-- TAB 4: Scan Devices -->
        <div id="tab-scan" class="tab-pane">
            <div class="card">
                <div class="card-header">
                    <h1>Scan Modbus LoRa Devices</h1>
                    <button type="button" class="btn-sm" id="btnSaveAllDiscovered" style="display: none; background: var(--success); border-color: var(--success); color: #fff; padding: 0.55rem 1rem;" onclick="saveAllDiscoveredToConfig()">
                        &#x2795; Save Discovered to Config
                    </button>
                </div>

                <p style="font-size: 0.88rem; color: var(--text-muted); margin-top: 0; margin-bottom: 1.25rem;">
                    Probe a range of Modbus Slave IDs (1–247) over LoRa to discover active STM32 nodes and inspect their Hardware ID, Valves, and Sensors.
                </p>

                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 1rem; margin-bottom: 1rem;">
                    <div class="form-group" style="margin: 0;">
                        <label>Start Slave ID</label>
                        <input type="number" id="scan_start_id" value="1" min="1" max="247">
                    </div>
                    <div class="form-group" style="margin: 0;">
                        <label>End Slave ID</label>
                        <input type="number" id="scan_end_id" value="10" min="1" max="247">
                    </div>
                    <div class="form-group" style="margin: 0;">
                        <label>Timeout / Node (ms)</label>
                        <input type="number" id="scan_timeout_ms" value="800" min="200" max="5000" step="100">
                    </div>
                </div>

                <div style="display: flex; gap: 0.75rem; flex-wrap: wrap; margin-bottom: 1.25rem;">
                    <button type="button" id="btnStartScan" class="btn-success" style="margin: 0; flex: 2; min-width: 160px;" onclick="startDeviceScan()">
                        &#x1F50D; Start Range Scan
                    </button>
                    <button type="button" id="btnScanConfigured" style="margin: 0; flex: 1; min-width: 160px; background: rgba(59, 130, 246, 0.2); border: 1px solid var(--primary);" onclick="scanConfiguredDevices()">
                        &#x26A1; Scan Configured Only
                    </button>
                    <button type="button" id="btnStopScan" style="margin: 0; flex: 1; min-width: 120px; background: var(--danger); display: none;" onclick="stopDeviceScan()">
                        &#x23F9; Stop Scan
                    </button>
                </div>

                <!-- Progress Bar -->
                <div id="scan_progress_wrap" style="display: none; margin-bottom: 1.25rem; background: rgba(255,255,255,0.03); padding: 0.85rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                    <div style="display: flex; justify-content: space-between; font-size: 0.85rem; margin-bottom: 0.5rem;">
                        <span id="scan_status_text" style="color: var(--text-main); font-weight: 600;">Scanning...</span>
                        <span id="scan_found_count" style="color: var(--success); font-weight: 700;">0 found</span>
                    </div>
                    <div style="width: 100%; height: 8px; background: var(--bg-color); border-radius: 999px; overflow: hidden;">
                        <div id="scan_progress_bar" style="width: 0%; height: 100%; background: linear-gradient(90deg, var(--primary), var(--success)); transition: width 0.2s ease;"></div>
                    </div>
                </div>

                <!-- Discovered Devices Table -->
                <div style="overflow-x: auto;">
                    <table class="telem-table">
                        <thead>
                            <tr>
                                <th>Slave ID</th>
                                <th>Status</th>
                                <th>Hardware ID</th>
                                <th>Valves (V1 / V2)</th>
                                <th>Sensors (S1 / S2)</th>
                                <th>TX Count</th>
                                <th style="text-align: right;">Actions</th>
                            </tr>
                        </thead>
                        <tbody id="scan_results_body">
                            <tr>
                                <td colspan="7" style="text-align: center; color: var(--text-muted); padding: 2rem;">
                                    Click <strong>Start Range Scan</strong> or <strong>Scan Configured Only</strong> to search for active LoRa Modbus devices.
                                </td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>

        <!-- TAB 5: MQTT <-> Modbus Bridge -->
        <div id="tab-mqtt" class="tab-pane">
            <!-- Status & Live Metrics Card -->
            <div class="card">
                <div class="card-header">
                    <h1>MQTT &#x2194; Modbus RTU Bridge</h1>
                    <span id="mqtt_status_badge" class="badge badge-unknown">CHECKING...</span>
                </div>

                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 0.85rem; margin-bottom: 1.25rem;">
                    <div style="background: rgba(255,255,255,0.03); padding: 0.85rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                        <div style="font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase;">Broker Endpoint</div>
                        <div id="mqtt_disp_broker" style="font-weight: 700; font-family: monospace; margin-top: 0.25rem;">127.0.0.1:1883</div>
                    </div>
                    <div style="background: rgba(255,255,255,0.03); padding: 0.85rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                        <div style="font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase;">Base Topic Prefix</div>
                        <div id="mqtt_disp_prefix" style="font-weight: 700; font-family: monospace; color: var(--primary); margin-top: 0.25rem;">lora/modbus</div>
                    </div>
                    <div style="background: rgba(255,255,255,0.03); padding: 0.85rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                        <div style="font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase;">Traffic (TX / RX)</div>
                        <div style="font-weight: 700; font-family: monospace; margin-top: 0.25rem;">
                            <span id="mqtt_disp_tx" style="color: #34d399;">0</span> TX / <span id="mqtt_disp_rx" style="color: #60a5fa;">0</span> RX
                        </div>
                    </div>
                    <div style="background: rgba(255,255,255,0.03); padding: 0.85rem 1rem; border-radius: 8px; border: 1px solid var(--border);">
                        <div style="font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase;">Connected Since</div>
                        <div id="mqtt_disp_since" style="font-weight: 600; font-size: 0.85rem; margin-top: 0.25rem;">Never</div>
                    </div>
                </div>

                <div id="mqtt_error_banner" style="display: none; margin-bottom: 1.25rem; padding: 0.65rem 0.9rem; border-radius: 8px; background: rgba(239, 68, 68, 0.15); border: 1px solid var(--danger); color: #fca5a5; font-size: 0.85rem;">
                </div>

                <form id="mqttConfigForm">
                    <div class="form-group checkbox-group" style="margin-top: 0; margin-bottom: 1.25rem;">
                        <input type="checkbox" id="mqtt_enabled" checked>
                        <label for="mqtt_enabled">Enable MQTT &#x2194; Modbus Bridge</label>
                    </div>

                    <div style="display: grid; grid-template-columns: 2fr 1fr; gap: 1rem;">
                        <div class="form-group">
                            <label>MQTT Broker Host / IP</label>
                            <input type="text" id="mqtt_host" value="127.0.0.1" placeholder="e.g. 127.0.0.1 or broker.hivemq.com">
                        </div>
                        <div class="form-group">
                            <label>Broker Port</label>
                            <input type="number" id="mqtt_port" value="1883" min="1" max="65535">
                        </div>
                    </div>

                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 1rem;">
                        <div class="form-group">
                            <label>Client ID</label>
                            <input type="text" id="mqtt_client_id" value="orangepi_lora_modbus">
                        </div>
                        <div class="form-group">
                            <label>Base Topic Prefix</label>
                            <input type="text" id="mqtt_topic_prefix" value="lora/modbus" placeholder="e.g. lora/modbus">
                        </div>
                    </div>

                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 1rem;">
                        <div class="form-group" style="margin-bottom: 0;">
                            <label>Username (Optional)</label>
                            <input type="text" id="mqtt_username" placeholder="Leave empty if anonymous">
                        </div>
                        <div class="form-group" style="margin-bottom: 0;">
                            <label>Password (Optional)</label>
                            <input type="password" id="mqtt_password" placeholder="Leave empty if anonymous">
                        </div>
                    </div>

                    <button type="submit">Save &amp; Reconnect MQTT Bridge</button>
                </form>
            </div>

            <!-- Interactive Test Publish & Command Simulator Card -->
            <div class="card">
                <h1>Test MQTT Publish / Command Bridge</h1>
                <p style="font-size: 0.85rem; color: var(--text-muted); margin-top: 0; margin-bottom: 1rem;">
                    Publish a message to the broker or test a bridged command on <code>&lt;prefix&gt;/cmd/...</code> (supports Slave ID <code>1..247</code> and Broadcast <code>0</code>):
                </p>

                <div style="display: flex; gap: 0.4rem; flex-wrap: wrap; margin-bottom: 1rem;">
                    <button type="button" class="pill-btn" onclick="applyMqttPreset('cmd/write_valve', {slave_id: 1, valve: 1, open: true})">Preset: Open Valve 1</button>
                    <button type="button" class="pill-btn broadcast-pill" onclick="applyMqttPreset('cmd/write_valve', {slave_id: 0, valve: 1, open: false})">Preset: Broadcast Close V1</button>
                    <button type="button" class="pill-btn" onclick="applyMqttPreset('cmd/read', {slave_id: 1, type: 'input_register', address: 0})">Preset: Read Sensor 1</button>
                    <button type="button" class="pill-btn" onclick="applyMqttPreset('cmd/read_all', {slave_id: 1})">Preset: Read All</button>
                    <button type="button" class="pill-btn" onclick="applyMqttPreset('cmd/probe', {slave_id: 1, timeout_ms: 1000})">Preset: Probe Slave 1</button>
                </div>

                <div style="display: grid; grid-template-columns: 1fr 2fr auto; gap: 0.75rem; align-items: end;">
                    <div class="form-group" style="margin: 0;">
                        <label style="font-size: 0.75rem;">Subtopic</label>
                        <input type="text" id="mqtt_test_subtopic" value="cmd/write_valve" style="font-family: monospace;">
                    </div>
                    <div class="form-group" style="margin: 0;">
                        <label style="font-size: 0.75rem;">JSON Payload</label>
                        <input type="text" id="mqtt_test_payload" value='{"slave_id": 1, "valve": 1, "open": true}' style="font-family: monospace;">
                    </div>
                    <button type="button" class="btn-success" style="margin: 0; width: auto; padding: 0.85rem 1.25rem;" onclick="sendMqttTestPublish()">Publish</button>
                </div>
            </div>

            <!-- Live MQTT Traffic Log & Topic Reference Card -->
            <div class="card">
                <div class="card-header">
                    <h1>Live MQTT Bridge Traffic</h1>
                    <button type="button" class="btn-sm" onclick="clearMqttLogs()">Clear Log</button>
                </div>

                <div style="overflow-x: auto; margin-bottom: 1.5rem;">
                    <table class="telem-table">
                        <thead>
                            <tr>
                                <th style="width: 90px;">Time</th>
                                <th style="width: 65px;">Dir</th>
                                <th>Topic</th>
                                <th>Payload</th>
                            </tr>
                        </thead>
                        <tbody id="mqtt_logs_body">
                            <tr>
                                <td colspan="4" style="text-align: center; color: var(--text-muted); padding: 1.5rem;">No MQTT traffic recorded yet.</td>
                            </tr>
                        </tbody>
                    </table>
                </div>

                <label>Supported MQTT Bridge Topics Reference</label>
                <div style="overflow-x: auto;">
                    <table class="telem-table" style="font-size: 0.82rem;">
                        <thead>
                            <tr>
                                <th>Direction</th>
                                <th>Topic Pattern</th>
                                <th>Example JSON Payload / Description</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                                <td><span class="badge badge-open">PUB (TX)</span></td>
                                <td><code>&lt;prefix&gt;/telemetry/&lt;slave_id&gt;</code></td>
                                <td>Auto-published whenever Modbus read/write updates telemetry</td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-open">PUB (TX)</span></td>
                                <td><code>&lt;prefix&gt;/response[/&lt;slave_id&gt;]</code></td>
                                <td>Command execution result (<code>{"status":"ok", ...}</code>)</td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/write_valve</code></td>
                                <td><code>{"slave_id": 1, "valve": 1, "open": true}</code> (use <code>slave_id: 0</code> for broadcast)</td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/write_coil</code></td>
                                <td><code>{"slave_id": 1, "address": 0, "state": true}</code></td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/write_register</code></td>
                                <td><code>{"slave_id": 1, "address": 4, "value": 1}</code></td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/write_multiple_coils</code></td>
                                <td><code>{"slave_id": 0, "address": 0, "values": [1, 1]}</code></td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/write_multiple_registers</code></td>
                                <td><code>{"slave_id": 0, "address": 4, "values": [1, 1]}</code></td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/read</code></td>
                                <td><code>{"slave_id": 1, "type": "input_register", "address": 0}</code></td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/read_all</code></td>
                                <td><code>{"slave_id": 1}</code> — polls all Coils, Discrete Inputs, Input &amp; Holding Regs</td>
                            </tr>
                            <tr>
                                <td><span class="badge badge-val">SUB (RX)</span></td>
                                <td><code>&lt;prefix&gt;/cmd/probe</code></td>
                                <td><code>{"slave_id": 1, "timeout_ms": 1000}</code> — probes device online status &amp; HW ID</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>
    
    <div id="toast" class="toast">Configuration saved successfully!</div>

    <script>
        let g_scanRunning = false;
        let g_scanAbort = false;
        let g_discoveredDevices = [];

        function switchTab(tabName) {
            const tabs = ['telemetry', 'controls', 'scan', 'mqtt', 'config'];
            tabs.forEach(t => {
                const pane = document.getElementById('tab-' + t);
                const btn = document.getElementById('tabBtn-' + t);
                if (pane) pane.classList.toggle('active', t === tabName);
                if (btn) btn.classList.toggle('active', t === tabName);
            });
            const slaveBar = document.getElementById('shared_slave_bar');
            if (slaveBar) {
                slaveBar.style.display = (tabName === 'config' || tabName === 'scan' || tabName === 'mqtt') ? 'none' : 'block';
            }
            if (tabName === 'mqtt') {
                pollMqttStatus();
            }
            try { localStorage.setItem('lora_active_tab', tabName); } catch (e) {}
        }

        function renderSlavePills(devices) {
            const container = document.getElementById('slave_pills_container');
            if (!container) return;
            const currentId = getTargetSlaveId();
            let html = `<button type="button" class="pill-btn broadcast-pill ${currentId === 0 ? 'active-pill' : ''}" data-slave="0" onclick="setTargetSlaveId(0)">Broadcast (0)</button>`;
            const list = (devices && devices.length > 0) ? devices : [1];
            list.forEach(id => {
                if (id > 0) {
                    html += `<button type="button" class="pill-btn ${currentId === id ? 'active-pill' : ''}" data-slave="${id}" onclick="setTargetSlaveId(${id})">Slave ${id}</button>`;
                }
            });
            container.innerHTML = html;
        }

        fetch('/api/config')
            .then(res => res.json())
            .then(data => {
                document.getElementById('frequency').value = data.frequency || 915000000;
                document.getElementById('tx_power').value = data.tx_power || 22;
                document.getElementById('spreading_factor').value = data.spreading_factor || 'SF9';
                document.getElementById('bandwidth').value = data.bandwidth || '125';
                document.getElementById('coding_rate').value = data.coding_rate || '4/6';
                document.getElementById('modbus_enabled').checked = (data.modbus_enabled !== undefined) ? data.modbus_enabled : true;
                const initSlave = (data.modbus_slave_id !== undefined) ? data.modbus_slave_id : 1;
                document.getElementById('modbus_slave_id').value = initSlave;
                const devList = (data.modbus_address_devices && data.modbus_address_devices.length > 0) ? data.modbus_address_devices : [1];
                document.getElementById('modbus_address_devices').value = devList.join(', ');
                renderSlavePills(devList);
                setTargetSlaveId(initSlave);
                document.getElementById('security_enabled').checked = (data.security_enabled !== undefined) ? data.security_enabled : true;
                document.getElementById('aes_key').value = data.aes_key || '2B7E151628AED2A6ABF7158809CF4F3C';
                document.getElementById('mqtt_enabled').checked = (data.mqtt_enabled !== undefined) ? data.mqtt_enabled : true;
                document.getElementById('mqtt_host').value = data.mqtt_host || '127.0.0.1';
                document.getElementById('mqtt_port').value = data.mqtt_port || 1883;
                document.getElementById('mqtt_client_id').value = data.mqtt_client_id || 'orangepi_lora_modbus';
                document.getElementById('mqtt_username').value = data.mqtt_username || '';
                document.getElementById('mqtt_password').value = data.mqtt_password || '';
                document.getElementById('mqtt_topic_prefix').value = data.mqtt_topic_prefix || 'lora/modbus';
            });

        try {
            const savedTab = localStorage.getItem('lora_active_tab');
            if (savedTab && ['telemetry', 'controls', 'scan', 'mqtt', 'config'].includes(savedTab)) {
                switchTab(savedTab);
            }
        } catch (e) {}

        function getTargetSlaveId() {
            const val = parseInt(document.getElementById('ctrl_slave_id').value, 10);
            return isNaN(val) ? 1 : val;
        }

        function formatSlaveIdLabel(id) {
            return (id === 0) ? '0 (Global Broadcast)' : String(id);
        }

        function onTargetSlaveChanged(val) {
            const id = parseInt(val, 10);
            const safeId = isNaN(id) ? 1 : id;
            document.getElementById('telem_slave_id_disp').innerText = formatSlaveIdLabel(safeId);
            const notice = document.getElementById('broadcast_notice');
            if (notice) {
                notice.style.display = (safeId === 0) ? 'block' : 'none';
            }
            document.querySelectorAll('#slave_pills_container .pill-btn').forEach(btn => {
                const pillSlave = parseInt(btn.getAttribute('data-slave'), 10);
                btn.classList.toggle('active-pill', pillSlave === safeId);
            });
        }

        function setTargetSlaveId(id) {
            document.getElementById('ctrl_slave_id').value = id;
            onTargetSlaveChanged(id);
        }

        function showToast(msg, isSuccess = true) {
            const toast = document.getElementById('toast');
            toast.innerText = msg;
            toast.style.background = isSuccess ? 'var(--success)' : 'var(--danger)';
            toast.classList.add('show');
            setTimeout(() => toast.classList.remove('show'), 3000);
        }

        function updateTelemetryUI(data) {
            if (!data) return;
            if (data.slave_id !== undefined && data.slave_id !== null) {
                document.getElementById('telem_slave_id_disp').innerText = formatSlaveIdLabel(data.slave_id);
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
            const slaveId = getTargetSlaveId();
            if (slaveId === 0) {
                showToast('Broadcast Address 0 is Write-Only (select Slave ID 1-247 to Read)', false);
                return;
            }
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
            const slaveId = getTargetSlaveId();
            if (slaveId === 0) {
                showToast('Broadcast Address 0 is Write-Only (select Slave ID 1-247 to Read)', false);
                return;
            }
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
            const slaveId = getTargetSlaveId();
            const targetDesc = (slaveId === 0) ? 'All Slaves (Broadcast 0)' : `Slave ${slaveId}`;
            fetch('/api/modbus/write_valve', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, valve: valveIndex, open: open })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`[${targetDesc}] Valve ${valveIndex} set to ${open ? 'OPEN (1)' : 'CLOSED (0)'}`);
                    pollTelemetry();
                } else {
                    showToast(`Write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

        function writeRegister() {
            const slaveId = getTargetSlaveId();
            const targetDesc = (slaveId === 0) ? 'All Slaves (Broadcast 0)' : `Slave ${slaveId}`;
            const addr = parseInt(document.getElementById('write_reg_addr').value, 10);
            const val = parseInt(document.getElementById('write_reg_val').value, 10);
            fetch('/api/modbus/write_register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, address: addr, value: val })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`[${targetDesc}] Register ${addr} written with ${val}`);
                    pollTelemetry();
                } else {
                    showToast(`Write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

        function setMultipleValves(v1Open, v2Open) {
            const slaveId = getTargetSlaveId();
            const targetDesc = (slaveId === 0) ? 'All Slaves (Broadcast 0)' : `Slave ${slaveId}`;
            fetch('/api/modbus/write_multiple_coils', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, address: 0, values: [v1Open, v2Open] })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`[${targetDesc}] Both valves set to ${v1Open ? 'OPEN' : 'CLOSED'}`);
                    pollTelemetry();
                } else {
                    showToast(`Multi-coil write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

        function writeMultipleRegisters() {
            const slaveId = getTargetSlaveId();
            const targetDesc = (slaveId === 0) ? 'All Slaves (Broadcast 0)' : `Slave ${slaveId}`;
            const startAddr = parseInt(document.getElementById('write_multi_reg_addr').value, 10);
            const rawVals = document.getElementById('write_multi_reg_vals').value;
            const values = rawVals.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n));
            if (values.length === 0) {
                showToast('Please enter at least one valid register value', false);
                return;
            }
            fetch('/api/modbus/write_multiple_registers', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ slave_id: slaveId, address: startAddr, values: values })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`[${targetDesc}] Wrote ${values.length} registers starting at ${startAddr}`);
                    pollTelemetry();
                } else {
                    showToast(`Multi-register write error: ${data.error}`, false);
                }
            }).catch(err => showToast(`Request failed`, false));
        }

        // --- Device Scan Logic ---

        function renderScanResults(isFinished = false) {
            const tbody = document.getElementById('scan_results_body');
            const saveAllBtn = document.getElementById('btnSaveAllDiscovered');
            if (!tbody) return;

            if (saveAllBtn) {
                saveAllBtn.style.display = (g_discoveredDevices.length > 0) ? 'inline-block' : 'none';
            }

            if (g_discoveredDevices.length === 0) {
                tbody.innerHTML = `<tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 2rem;">${
                    isFinished
                        ? 'Scan complete — no online Modbus devices found in the scanned range.'
                        : 'Scanning... waiting for active devices to respond.'
                }</td></tr>`;
                return;
            }

            const renderValveBadge = (v, label) => {
                if (v === 1) return `<span class="badge badge-open">${label}: OPEN</span>`;
                if (v === 0) return `<span class="badge badge-closed">${label}: CLOSED</span>`;
                return `<span class="badge badge-unknown">${label}: ?</span>`;
            };

            let html = '';
            g_discoveredDevices.forEach(dev => {
                const hwHex = '0x' + (dev.hw_id >>> 0).toString(16).toUpperCase().padStart(8, '0');
                const s1 = (dev.sensor1 >= 0) ? `${(dev.sensor1 / 10.0).toFixed(1)} °C` : '?';
                const s2 = (dev.sensor2 >= 0) ? `${dev.sensor2} hPa` : '?';
                html += `<tr>
                    <td><strong style="color: var(--primary); font-size: 1rem;">#${dev.slave_id}</strong></td>
                    <td><span class="badge badge-open">ONLINE</span></td>
                    <td><span class="badge badge-val">${hwHex}</span></td>
                    <td>${renderValveBadge(dev.valve1, 'V1')} ${renderValveBadge(dev.valve2, 'V2')}</td>
                    <td><span class="badge badge-val">${s1} / ${s2}</span></td>
                    <td><code>${dev.tx_count}</code></td>
                    <td style="text-align: right; white-space: nowrap;">
                        <button type="button" class="btn-sm" style="margin-right: 0.35rem;" onclick="selectScannedDevice(${dev.slave_id})">Select Target</button>
                        <button type="button" class="btn-sm" style="background: rgba(16, 185, 129, 0.15); border-color: #10b981; color: #34d399;" onclick="addDeviceToConfig(${dev.slave_id})">+ Config</button>
                    </td>
                </tr>`;
            });
            tbody.innerHTML = html;
        }

        async function runScanList(slaveIds, timeoutMs) {
            if (g_scanRunning || !slaveIds || slaveIds.length === 0) return;
            g_scanRunning = true;
            g_scanAbort = false;
            g_discoveredDevices = [];

            const btnStart = document.getElementById('btnStartScan');
            const btnConf = document.getElementById('btnScanConfigured');
            const btnStop = document.getElementById('btnStopScan');
            const progWrap = document.getElementById('scan_progress_wrap');
            const progBar = document.getElementById('scan_progress_bar');
            const statusText = document.getElementById('scan_status_text');
            const foundCount = document.getElementById('scan_found_count');

            if (btnStart) btnStart.disabled = true;
            if (btnConf) btnConf.disabled = true;
            if (btnStop) btnStop.style.display = 'inline-block';
            if (progWrap) progWrap.style.display = 'block';
            if (progBar) progBar.style.width = '0%';
            if (foundCount) foundCount.innerText = '0 found';

            renderScanResults(false);

            const total = slaveIds.length;
            for (let i = 0; i < total; i++) {
                if (g_scanAbort) break;
                const id = slaveIds[i];
                const pct = Math.round(((i) / total) * 100);
                if (progBar) progBar.style.width = `${pct}%`;
                if (statusText) statusText.innerText = `Probing Slave ID ${id} (${i + 1} of ${total})...`;

                try {
                    const res = await fetch('/api/modbus/probe', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ slave_id: id, timeout_ms: timeoutMs })
                    });
                    const data = await res.json();
                    if (data && data.online) {
                        g_discoveredDevices.push(data);
                        if (foundCount) foundCount.innerText = `${g_discoveredDevices.length} found`;
                        renderScanResults(false);
                    }
                } catch (err) {
                    console.debug('Probe failed for slave', id, err);
                }
            }

            if (progBar) progBar.style.width = '100%';
            if (statusText) {
                statusText.innerText = g_scanAbort
                    ? `Scan stopped (${g_discoveredDevices.length} device(s) found)`
                    : `Scan finished (${total} address(es) probed, ${g_discoveredDevices.length} online)`;
            }

            renderScanResults(true);
            g_scanRunning = false;
            if (btnStart) btnStart.disabled = false;
            if (btnConf) btnConf.disabled = false;
            if (btnStop) btnStop.style.display = 'none';
        }

        function startDeviceScan() {
            let startId = parseInt(document.getElementById('scan_start_id').value, 10) || 1;
            let endId = parseInt(document.getElementById('scan_end_id').value, 10) || 10;
            let timeoutMs = parseInt(document.getElementById('scan_timeout_ms').value, 10) || 800;

            if (startId < 1) startId = 1;
            if (endId > 247) endId = 247;
            if (startId > endId) {
                const tmp = startId;
                startId = endId;
                endId = tmp;
            }
            const list = [];
            for (let id = startId; id <= endId; id++) {
                list.push(id);
            }
            runScanList(list, timeoutMs);
        }

        function scanConfiguredDevices() {
            const devicesStr = document.getElementById('modbus_address_devices').value;
            const list = devicesStr.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n) && n >= 1 && n <= 247);
            if (list.length === 0) {
                showToast('No configured devices in Address Devices list', false);
                return;
            }
            const timeoutMs = parseInt(document.getElementById('scan_timeout_ms').value, 10) || 800;
            runScanList(list, timeoutMs);
        }

        function stopDeviceScan() {
            g_scanAbort = true;
            showToast('Stopping scan after current address...', false);
        }

        function selectScannedDevice(slaveId) {
            setTargetSlaveId(slaveId);
            switchTab('telemetry');
            showToast(`Selected Slave ID ${slaveId} as active target`);
        }

        function addDeviceToConfig(slaveId) {
            const input = document.getElementById('modbus_address_devices');
            let current = input.value.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n) && n > 0);
            if (!current.includes(slaveId)) {
                current.push(slaveId);
                current.sort((a, b) => a - b);
            }
            input.value = current.join(', ');
            renderSlavePills(current);
            document.getElementById('configForm').dispatchEvent(new Event('submit'));
            showToast(`Slave ${slaveId} added to Address Devices & saved!`);
        }

        function saveAllDiscoveredToConfig() {
            if (g_discoveredDevices.length === 0) return;
            const input = document.getElementById('modbus_address_devices');
            let current = input.value.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n) && n > 0);
            g_discoveredDevices.forEach(d => {
                if (d.slave_id > 0 && !current.includes(d.slave_id)) {
                    current.push(d.slave_id);
                }
            });
            current.sort((a, b) => a - b);
            input.value = current.join(', ');
            renderSlavePills(current);
            document.getElementById('configForm').dispatchEvent(new Event('submit'));
            showToast(`Saved ${g_discoveredDevices.length} discovered device(s) to Config!`);
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
                modbus_address_devices: devices,
                security_enabled: document.getElementById('security_enabled').checked,
                aes_key: document.getElementById('aes_key').value.trim().toUpperCase(),
                mqtt_enabled: document.getElementById('mqtt_enabled').checked,
                mqtt_host: document.getElementById('mqtt_host').value.trim() || '127.0.0.1',
                mqtt_port: parseInt(document.getElementById('mqtt_port').value, 10) || 1883,
                mqtt_client_id: document.getElementById('mqtt_client_id').value.trim() || 'orangepi_lora_modbus',
                mqtt_username: document.getElementById('mqtt_username').value.trim(),
                mqtt_password: document.getElementById('mqtt_password').value,
                mqtt_topic_prefix: document.getElementById('mqtt_topic_prefix').value.trim() || 'lora/modbus'
            };

            fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(config, null, 4)
            }).then(() => {
                renderSlavePills(devices);
                showToast('Configuration saved successfully!');
            });
        });

        // --- MQTT Bridge UI Logic ---

        function escapeHtml(str) {
            if (!str) return '';
            return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }

        function pollMqttStatus() {
            fetch('/api/mqtt/status')
                .then(res => res.json())
                .then(st => {
                    const badge = document.getElementById('mqtt_status_badge');
                    const dot = document.getElementById('mqtt_tab_dot');
                    if (badge && dot) {
                        if (!st.enabled) {
                            badge.className = 'badge badge-closed';
                            badge.innerText = 'DISABLED';
                            dot.style.background = '#9ca3af';
                        } else if (st.connected) {
                            badge.className = 'badge badge-open';
                            badge.innerText = 'CONNECTED';
                            dot.style.background = '#10b981';
                        } else {
                            badge.className = 'badge';
                            badge.style.background = '#7f1d1d';
                            badge.style.color = '#fca5a5';
                            badge.innerText = 'DISCONNECTED';
                            dot.style.background = '#ef4444';
                        }
                    }

                    document.getElementById('mqtt_disp_broker').innerText = `${st.broker_host || '127.0.0.1'}:${st.broker_port || 1883}`;
                    document.getElementById('mqtt_disp_prefix').innerText = st.topic_prefix || 'lora/modbus';
                    document.getElementById('mqtt_disp_tx').innerText = st.tx_count || 0;
                    document.getElementById('mqtt_disp_rx').innerText = st.rx_count || 0;
                    document.getElementById('mqtt_disp_since').innerText = st.last_connected_time || 'Never';

                    const errBanner = document.getElementById('mqtt_error_banner');
                    if (errBanner) {
                        if (st.last_error && st.last_error.length > 0) {
                            errBanner.style.display = 'block';
                            errBanner.innerText = 'Last Broker Notice: ' + st.last_error;
                        } else {
                            errBanner.style.display = 'none';
                        }
                    }

                    const tbody = document.getElementById('mqtt_logs_body');
                    if (tbody && st.recent_logs) {
                        if (st.recent_logs.length === 0) {
                            tbody.innerHTML = '<tr><td colspan="4" style="text-align: center; color: var(--text-muted); padding: 1.5rem;">No MQTT traffic recorded yet.</td></tr>';
                        } else {
                            let html = '';
                            st.recent_logs.forEach(item => {
                                const dirBadge = item.direction === 'TX'
                                    ? '<span class="badge badge-open">TX</span>'
                                    : '<span class="badge badge-val">RX</span>';
                                html += `<tr>
                                    <td><code>${escapeHtml(item.timestamp)}</code></td>
                                    <td>${dirBadge}</td>
                                    <td><code>${escapeHtml(item.topic)}</code></td>
                                    <td style="font-family: monospace; font-size: 0.8rem; word-break: break-all;">${escapeHtml(item.payload)}</td>
                                </tr>`;
                            });
                            tbody.innerHTML = html;
                        }
                    }
                })
                .catch(err => console.debug('MQTT status poll idle', err));
        }

        document.getElementById('mqttConfigForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const payload = {
                mqtt_enabled: document.getElementById('mqtt_enabled').checked,
                mqtt_host: document.getElementById('mqtt_host').value.trim() || '127.0.0.1',
                mqtt_port: parseInt(document.getElementById('mqtt_port').value, 10) || 1883,
                mqtt_client_id: document.getElementById('mqtt_client_id').value.trim() || 'orangepi_lora_modbus',
                mqtt_username: document.getElementById('mqtt_username').value.trim(),
                mqtt_password: document.getElementById('mqtt_password').value,
                mqtt_topic_prefix: document.getElementById('mqtt_topic_prefix').value.trim() || 'lora/modbus'
            };

            fetch('/api/mqtt/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast('MQTT Bridge configuration saved & reconnecting!');
                    setTimeout(pollMqttStatus, 600);
                } else {
                    showToast('Failed to save MQTT configuration', false);
                }
            }).catch(() => showToast('Request failed', false));
        });

        function applyMqttPreset(subtopic, payloadObj) {
            document.getElementById('mqtt_test_subtopic').value = subtopic;
            document.getElementById('mqtt_test_payload').value = JSON.stringify(payloadObj);
        }

        function sendMqttTestPublish() {
            const subtopic = document.getElementById('mqtt_test_subtopic').value.trim();
            const payload = document.getElementById('mqtt_test_payload').value;
            if (!subtopic) {
                showToast('Please enter a subtopic', false);
                return;
            }
            fetch('/api/mqtt/publish', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ subtopic: subtopic, payload: payload })
            }).then(res => res.json()).then(data => {
                if (data.status === 'ok') {
                    showToast(`Published to ${subtopic}`);
                    setTimeout(pollMqttStatus, 400);
                } else {
                    showToast(data.error || 'MQTT publish failed (check broker connection)', false);
                }
            }).catch(() => showToast('Request failed', false));
        }

        function clearMqttLogs() {
            fetch('/api/mqtt/clear_logs', { method: 'POST' })
                .then(() => pollMqttStatus());
        }

        // Initialize telemetry & MQTT status polling every 2.5s
        setInterval(() => {
            pollTelemetry();
            pollMqttStatus();
        }, 2500);
        pollTelemetry();
        pollMqttStatus();
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

    svr.Post("/api/modbus/write_multiple_coils", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int address = j.value("address", 0);
            auto vals_json = j.at("values");
            std::vector<uint8_t> values;
            for (const auto& item : vals_json) {
                if (item.is_boolean()) {
                    values.push_back(item.get<bool>() ? 1 : 0);
                } else {
                    values.push_back(item.get<int>() != 0 ? 1 : 0);
                }
            }
            bool success = modbus_write_multiple_coils(slave_id, address, (int)values.size(), values.data());
            if (success) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to write multiple coils";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/write_multiple_registers", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int address = j.value("address", 0);
            std::vector<uint16_t> values = j.at("values").get<std::vector<uint16_t>>();
            bool success = modbus_write_multiple_holding_registers(slave_id, address, (int)values.size(), values.data());
            if (success) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                std::string err = get_last_modbus_error();
                if (err.empty()) err = "Failed to write multiple registers";
                res.status = 500;
                res.set_content(nlohmann::json({{"status", "error"}, {"error", err}}).dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/modbus/probe", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            int slave_id = j.value("slave_id", 1);
            int timeout_ms = j.value("timeout_ms", 800);
            ModbusDeviceProbe info;
            bool online = modbus_probe_slave(slave_id, timeout_ms, info);
            nlohmann::json resp = {
                {"status", "ok"},
                {"slave_id", info.slave_id},
                {"online", online},
                {"hw_id", info.hw_id},
                {"tx_count", info.tx_count},
                {"valve1", info.valve1},
                {"valve2", info.valve2},
                {"sensor1", info.sensor1},
                {"sensor2", info.sensor2},
                {"slave_id_reg", info.slave_id_reg}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid request JSON\"}", "application/json");
        }
    });

    // --- MQTT Bridge REST API Endpoints ---

    svr.Get("/api/mqtt/status", [](const httplib::Request& req, httplib::Response& res) {
        MqttBridgeStatus st = get_mqtt_bridge_status();
        nlohmann::json logs_arr = nlohmann::json::array();
        for (const auto& entry : st.recent_logs) {
            logs_arr.push_back({
                {"timestamp", entry.timestamp},
                {"direction", entry.direction},
                {"topic", entry.topic},
                {"payload", entry.payload}
            });
        }
        nlohmann::json resp = {
            {"enabled", st.enabled},
            {"connected", st.connected},
            {"broker_host", st.broker_host},
            {"broker_port", st.broker_port},
            {"client_id", st.client_id},
            {"topic_prefix", st.topic_prefix},
            {"tx_count", st.tx_count},
            {"rx_count", st.rx_count},
            {"last_error", st.last_error},
            {"last_connected_time", st.last_connected_time},
            {"recent_logs", logs_arr}
        };
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/api/mqtt/config", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            if (j.contains("mqtt_enabled")) g_config.mqtt_enabled = j["mqtt_enabled"].get<bool>();
            if (j.contains("mqtt_host")) g_config.mqtt_host = j["mqtt_host"].get<std::string>();
            if (j.contains("mqtt_port")) g_config.mqtt_port = j["mqtt_port"].get<int>();
            if (j.contains("mqtt_client_id")) g_config.mqtt_client_id = j["mqtt_client_id"].get<std::string>();
            if (j.contains("mqtt_username")) g_config.mqtt_username = j["mqtt_username"].get<std::string>();
            if (j.contains("mqtt_password")) g_config.mqtt_password = j["mqtt_password"].get<std::string>();
            if (j.contains("mqtt_topic_prefix")) g_config.mqtt_topic_prefix = j["mqtt_topic_prefix"].get<std::string>();

            g_config.save_to_file("config.json");
            reload_mqtt_bridge();
            res.set_content("{\"status\": \"ok\"}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid MQTT config JSON\"}", "application/json");
        }
    });

    svr.Post("/api/mqtt/publish", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            std::string subtopic = j.value("subtopic", "");
            std::string payload = j.value("payload", "{}");
            bool retain = j.value("retain", false);

            if (mqtt_publish(subtopic, payload, retain)) {
                res.set_content("{\"status\": \"ok\"}", "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"status\": \"error\", \"error\": \"MQTT broker not connected or bridge disabled\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"status\": \"error\", \"error\": \"Invalid publish request JSON\"}", "application/json");
        }
    });

    svr.Post("/api/mqtt/clear_logs", [](const httplib::Request& req, httplib::Response& res) {
        clear_mqtt_bridge_logs();
        res.set_content("{\"status\": \"ok\"}", "application/json");
    });

    std::cout << "\n>>> Starting Embedded C++ Web Server on http://0.0.0.0:8080 <<<\n" << std::endl;
    svr.listen("0.0.0.0", 8080);
}

