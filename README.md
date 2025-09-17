# v4l2_test

Minimal C++ V4L2 frame grabber. Prints or logs each frame's timestamp and FPS. 

## Build

```sh
make
```

## Usage

```sh
./v4l2_test /dev/videoX [posix] [logfilename]
```

- `/dev/videoX` : Video device (required)
- `print`       : Timestamp print if added (optional). If not added, the timestamp will not print in terminal
- `posix`       : Print POSIX epoch timestamp (optional). If not added, the timestamp captured according to CLOCK_MONOTONIC time. 
- `logfilename` : Log output to file (optional)

**Examples:**
- Print to terminal:  
  `./v4l2_test /dev/video0`
- Print POSIX time to terminal:  
  `./v4l2_test /dev/video0 posix`
- Log to file:  
  `./v4l2_test /dev/video0 posix cam0`
