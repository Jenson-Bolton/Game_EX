# Game_EX

## Build Instructions

### 1. Clone The Repository

First clone the repository by running 
```
git clone https://github.com/Jenson-Bolton/Game_EX.git
cd Game_EX
git submodule update --init --recursive
```


### 2. Bootstrap vcpkg & EMSDK

#### Windows (PowerShell)
```
.\external\vcpkg\bootstrap-vcpkg.bat
Unblock-File .\tools\setup_emsdk_win.ps1
.\tools\setup_emsdk_win.ps1
```

#### macOS / Linux (bash/zsh)
```
./external/vcpkg/bootstrap-vcpkg.sh
./tools/setup_emsdk_unix.sh
```


### 3. Build

#### Windows (Visual Studio)
```
cmake --preset win-debug
cmake --build --preset win-debug
```

### Apple Silicone
```
cmake --preset mac-arm64-debug
cmake --build --preset mac-arm64-debug
```

### Universal macOS release
```
cmake --preset mac-universal-release
cmake --build --preset mac-universal-release
```

### Linux
```
cmake --preset linux-debug
cmake --build --preset linux-debug
```

### Web (Emscripten)
```
cmake --preset web-debug
cmake --build --preset web-debug
```


## General specification (floating ideas)

Cityscape based in London
Around St Pauls

3D game, inspiref by graphics from
GTA 3, Max Payne, early 2000s games

Idea of the game
is to
just walk around London and explore