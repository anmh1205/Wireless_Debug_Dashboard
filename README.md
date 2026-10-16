# Wireless Debug Dashboard

An ESP32-hosted wireless dashboard for viewing device and robot debug data in a browser. The firmware receives values over UART and distributes updates through WebSocket and a lightweight MQTT broker, without requiring a separate computer to host the interface.

## How it works

The ESP32 creates a Wi-Fi access point and serves a small web application from its SPIFFS filesystem. A connected device sends serial records, which the firmware associates with configured dashboard entries. The ESP32 then forwards updates to connected browser clients over WebSocket and publishes the values over MQTT.

```text
Robot / embedded device -- UART --> ESP32
                                  |-- Wi-Fi access point + HTTP dashboard
                                  |-- WebSocket updates to browsers
                                  '-- MQTT broker on port 1883
```

This is a project-specific debugging tool, not a general-purpose replacement for a full MQTT broker or an industrial telemetry platform.

## Features found in the source

- Access-point mode and HTTP server hosted on ESP32.
- Serial data handling through `Serial2` at 115200 baud.
- WebSocket endpoint `/ws` for updating connected dashboard clients.
- MQTT broker implementation and per-value update topics.
- SPIFFS storage for dashboard box configuration.
- Browser interface with HTML, JavaScript, and CSS assets.

## Hardware and requirements

The PlatformIO project provides two build environments:

| Environment | Declared target |
| --- | --- |
| `featheresp32` | `esp32dev` board |
| `4d_systems_esp32s3_gen4_r8n16` | 4D Systems ESP32-S3 board |

Choose an environment that matches the actual board. The source is built with the Arduino framework and uses ESPAsyncWebServer, AsyncTCP, ArduinoJson, and an MQTT client library.

You will need [PlatformIO](https://platformio.org/), a compatible ESP32 board, a serial-connected device that produces the expected data, and a browser capable of connecting to the ESP32 access point.

## Build and upload

From the repository root, for the generic ESP32 environment:

```sh
pio run -e featheresp32
pio run -e featheresp32 -t upload
pio run -e featheresp32 -t uploadfs
```

The last command uploads the web assets in [`data/`](data/) to SPIFFS. For the alternative ESP32-S3 board, replace the environment with `4d_systems_esp32s3_gen4_r8n16`. Verify the serial port, board definition, and UART wiring before flashing.

### Connect and view data

1. Inspect and configure the access-point settings in [`src/main.cpp`](src/main.cpp). Change the default credentials before use.
2. Flash the firmware and upload the SPIFFS filesystem.
3. Connect a computer or phone to the ESP32's Wi-Fi access point.
4. Open the AP's configured local IP address in a browser. The default configuration uses `192.168.4.1`.
5. Send a line of UART data in the expected `id,value` format, terminated with a carriage return (`\r`). The firmware processes IDs that are present in its dashboard box configuration.

The WebSocket and MQTT behavior is implemented in the source; validate throughput and error handling on your actual board and serial link.

## Project structure

| Path | Purpose |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Initialization, AP, HTTP/WebSocket server, and serial processing |
| [`src/MQTTBroker.cpp`](src/MQTTBroker.cpp) | Embedded MQTT broker implementation |
| [`src/WiFiManager.cpp`](src/WiFiManager.cpp) | Wi-Fi management functions |
| [`data/`](data/) | Browser interface and dashboard scripts |
| [`platformio.ini`](platformio.ini) | PlatformIO environments and dependencies |

## Security and limitations

The AP configuration currently contains a default password in firmware source. Change it before deployment; do not expose an unprotected debug interface on an untrusted network. MQTT and HTTP on this local AP should be treated as development/debugging interfaces unless independently hardened. No repository-wide license is currently declared.
