# Tiny JS Game

Tiny JS Game is a small game engine written in pure C. It uses JavaScript files to create and run games. This project aims to provide a simple and lightweight framework for game development, leveraging the power and flexibility of JavaScript for game scripting.

The goal of this project is to make producing handheld game consoles super easy and approachable.

## Features

- Written in pure C for performance and portability
- Uses JavaScript for game logic and scripting
- Simple and lightweight design
- Web-compatible API for console logging

## Getting Started

### Prerequisites

- GCC (or any C compiler)
- SDL3
- Make

### Building the Project

To build the project, run the following commands:


Clone the project

```bash
git clone git@github.com:dmikey/tiny-js-games.git
```

```bash
cd tiny-js-games.git
git submodule init && git submodule update
```

Build SDL first. Needed for media. And graphics.

```bash
cd lib/SDL
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
sudo cmake --install . --config Release
```

Build the main project

```bash
make
```
