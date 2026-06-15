# LocalChat  
*A lightweight, cross‑platform peer‑to‑peer console chat application written in pure C.*  

---

## Features
- Real‑time text messaging  
- File transfer support (`/send <filename>`)  
- Fully cross‑platform (Linux + Windows)  
- Zero external dependencies  
- Simple and clean console interface  

---

## Build

### Linux  
```bash
gcc -o localchat chat.c -pthread
```

### Windows (MinGW / MSYS2 / Git Bash)  
```bash
gcc -o localchat.exe chat.c -lws2_32
```

---

## Usage

### Host (Server)  
```bash
./localchat
```

### Client  
```bash
./localchat <IP_ADDRESS>
```

#### Example  

| Terminal | Action |
|----------|--------|
| **1** (Host) | `./localchat` |
| **2** (Client) | `./localchat 192.168.1.105` |

---

## Commands

- Type any message and press *Enter* to send  
- `/send example.txt` – send a file  
- `/quit` – exit the program  

---

## Default Port
```
6847
```

---

## Limitations
- Only supports two users (peer‑to‑peer)  
- No encryption (use only on trusted local networks)  
- Basic file transfer (one file at a time)  
