# EndlessPeak
A simple server for 2D games using WebSocket

## Features
### Networking
- Asynchronous network processing (WebSocket using Boost.Asio/Beast)
- Secure communication via OpenSSL

### Game Mechanics
- Game physics:
  - Collision
  - Gravity
- Player abilities:
  - Walk
  - Jump

### Data & Logging
- JSON serialization using nlohmann/json
- Logging via spdlog
- Database access via MariaDB Connector/C

## Requirements
- C++20 compatible compiler
- CMake >= 3.20
- Supported compilers: GCC, Clang

## Building the Project
```bash
git clone git@github.com:kenka1/EndlessPeakServer.git
cd ./EndlessPeakServer/
cmake -B build
cmake --build build -j4
./build/src/server ./config/config.json
```

## Simple visualisation client
- git@github.com:kenka1/EndlessPeakClient.git
