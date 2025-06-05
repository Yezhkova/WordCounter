# High-Performance Multithreaded Word Counter in C++

This project implements a high-performance, multithreaded word frequency counter for large text files using low-level Linux system calls and aligned I/O (`O_DIRECT`). It efficiently reads and processes massive files in parallel, even when the file size exceeds available RAM.

## Features

- Multithreaded processing (automatic CPU core detection)
- Uses `pread` and `posix_memalign` for aligned direct I/O
- Avoids word fragmentation by handling boundaries between chunks
- Outputs total and unique word counts
- Benchmarks total processing time

## How It Works

- The file is divided into chunks of size `BLOCKSIZE = (4096 * 2)^2 = 65536 * 4096 = 256MB`
- Each chunk includes a margin (`FRAME`) on both ends to capture partially split words
- Threads read chunks concurrently and build local hash maps
- Final global map is built by merging all local maps
- Uses `O_DIRECT` for bypassing the page cache (aligned I/O)

## Requirements

- **Linux system** with support for `O_DIRECT`
- **C++17 or newer**
- Compiled with a modern compiler: `g++`, `clang++`

## Build & Run

### 1. Compile

```bash
g++ -O3 -std=c++17 -pthread word_counter.cpp -o word_counter

### 2. Run
```bash
./word_counter <your_large_text_file.txt>
