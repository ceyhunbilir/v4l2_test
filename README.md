# v4l2_test

Minimal C++ V4L2 frame grabber. Prints or logs each frame's timestamp and FPS based on configuration settings.

## Build

```sh
make
```

## Configuration

All settings are configured via `config.yaml`:

```yaml
print_to_console: true          # Enable/disable timestamp printing to console
use_posix_format: false         # Use POSIX epoch time instead of CLOCK_MONOTONIC
enable_file_logging: false      # Enable/disable logging to file
log_filename: ""                # Log filename (empty = no logging)
show_fps: true                  # Show FPS in output
```

## Usage

```sh
./v4l2_test /dev/videoX
```

- `/dev/videoX` : Video device (required, command line argument)
- All other settings are configured in `config.yaml`

**Examples:**
- Basic usage:  
  `./v4l2_test /dev/video0`
- With GMSL camera:  
  `./v4l2_test /dev/gmsl/tier4-isx021-cam1`

**Configuration Examples:**
- To print timestamps: Set `print_to_console: true` in config.yaml
- To use POSIX time: Set `use_posix_format: true` in config.yaml  
- To log to file: Set `enable_file_logging: true` and `log_filename: "output.log"` in config.yaml
