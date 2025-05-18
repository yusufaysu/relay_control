# ESP32 Smart Home Control System

This project is an **ESP32**-based smart home automation system featuring Ethernet (ENC28J60) connectivity, an MCP23017 GPIO expander, and a modern web-based control panel. The system supports up to 32 inputs and 32 outputs, allowing users to easily manage lighting and shutter groups through a user-friendly interface.

## Features

- **Fast and stable Ethernet (ENC28J60) connectivity**
- **MCP23017 GPIO expander** with support for 32 inputs and 32 outputs
- **SPIFFS file system** for hosting web interface files directly on the ESP32
- **Modern, responsive, dark-blue themed web panel**
- **User-defined group and output management**
- **Dedicated output management for shutter control**
- **Secure session management (cookie-based login)**
- **Frontend-backend integration via JSON-based API**
- **Secure and fast data processing with cJSON**
- **Security measures for stack overflow and large JSON logging**
- **Easy setup and customization**

## Technologies Used

- **ESP-IDF**: ESP32 development framework
- **C++**: Hardware and backend code
- **HTML/CSS/JavaScript**: Modern web interface (panel.html, login.html)
- **SPIFFS**: Storing web files on the ESP32
- **cJSON**: JSON data processing
- **Ethernet (ENC28J60)**: Wired network connection
- **MCP23017**: I2C-based GPIO expander

## Project Structure

```
├── main/
│   ├── main.cpp
│   └── inc/
│       ├── ethernet/
│       │   ├── ethernet.cpp / ethernet.hpp
│       ├── mcp23017/
│       │   ├── mcp23017.cpp / mcp23017.hpp
│       └── webserver/
│           ├── webserver.cpp / webserver.hpp
├── spiffs/
│   ├── panel.html
│   └── login.html
├── CMakeLists.txt
├── README.md
└── ...
```

## Setup

1. **Install the ESP-IDF environment**  
   [ESP-IDF Setup Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

2. **Check dependencies**  
   - The cJSON library is included in the project.
   - All necessary settings are configured in `CMakeLists.txt` and `idf_component.yml`.

3. **Upload SPIFFS files**  
   Upload `panel.html` and `login.html` from the `spiffs` folder to the ESP32.

4. **Build and flash the project**
   ```
   idf.py build
   idf.py -p [PORT] flash
   ```

5. **Start the device and access the web interface**  
   Open the device's IP address in your browser via Ethernet connection.

## Web Panel Features

- **Input/Output Groups:**  
  Add, name, and manage unlimited lighting and shutter groups.
- **Shutter Control:**  
  Each shutter group is assigned two outputs (up/down), and their names can be changed.
- **User-Friendly Interface:**  
  All operations are managed with localStorage and communicate with the backend via a JSON-based API.
- **Secure Login:**  
  Access to the panel is restricted until login is completed via login.html.

## API Endpoints

- `GET /api/outputs` — Retrieves output states
- `GET /api/inputs` — Retrieves input states
- `POST /api/save_groups` — Saves group configuration
- (See the code for detailed JSON formats and examples.)

## Security and Stability

- Session control is managed via cookies.
- Security measures are in place for large JSON logging and stack overflow.
- All inputs and API requests are validated.

## Contribution and License

You can contribute by submitting a pull request or opening an issue.  
For license information, please check the LICENSE file.
