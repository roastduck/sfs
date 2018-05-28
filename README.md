# Secure File System

# Dependencies

- FUSE >= 2.6 (Use `apt-get install libfuse-dev` on Ubuntu 16.04 will get you a FUSE 2.6)
- libgit2 (Use `apt-get install libgit2-dev` on Ubuntu)
- CMake >= 2.8.12

# Build

```sh
cmake . # Or `cmake -DCMAKE_BUILD_TYPE=Debug .` when developing
make
```

# Run

```sh
cp config.json.example config.json
vim config.json # Edit mounting point etc.
bin/sfs config.json
```

# Pitfalls

- If you get "Transport endpoint is not connected" error message after SFS crashes, you have to manually unmount your mounting point. Simply exceute `sudo umount /path/to/your/mounting/point`.

