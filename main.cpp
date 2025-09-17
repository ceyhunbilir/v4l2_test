#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>
#include <sys/mman.h>
#include <cstring>
#include <chrono>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

// Buffer structure for memory-mapped frame data
struct buffer {
    void   *start;
    size_t length;
};

// Flags for timestamp mode and logging
bool TIMESTAMP_MODE = false;     // If true, print timestamps
bool CONVERT_POSIX_TIME = false; // If true, print POSIX epoch timestamp
bool LOG_TO_FILE = false;        // If true, log to file instead of stdout

// Helper function to generate log file name based on device
std::string get_log_filename(const char* dev_name) {
    // Example: /dev/video0 -> cam0.log
    std::string dev(dev_name);
    size_t pos = dev.find_last_not_of("0123456789");
    std::string cam_id = dev.substr(pos + 1);
    return "cam" + cam_id + ".log";
}

int main(int argc, char* argv[]) {
    // Require at least one argument (video device)
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " /dev/videoX [timestamp] [posix] [file]\n";
        std::cerr << "  /dev/videoX : Video device (required)\n";
        std::cerr << "  timestamp   : Enable timestamp printing (optional)\n";
        std::cerr << "  posix       : Print POSIX epoch timestamp (optional)\n";
        std::cerr << "  file        : Log output to timestamps.log (optional)\n";
        return 1;
    }

    // Select video device (default: /dev/video0)
    const char* dev_name = argv[1];

    // Check for timestamp flag in arguments
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "timestamp") {
            TIMESTAMP_MODE = true;
            break;
        }
    }

    // Open video device
    int fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("Cannot open video device");
        return 1;
    }


    // Request buffer(s) from the device
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("Requesting Buffer");
        close(fd);
        return 1;
    }

    // Prepare V4L2 buffer
    v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        perror("Querying Buffer");
        close(fd);
        return 1;
    }

    // Map buffer to user space
    buffer framebuf;
    framebuf.length = buf.length;
    framebuf.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (framebuf.start == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // Queue buffer for capture
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        perror("Queue Buffer");
        close(fd);
        return 1;
    }

    // Start video streaming
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Start Capture");
        close(fd);
        return 1;
    }

    // Set flags from command line arguments
    // Usage: ./v4l2_npl54_test /dev/video0 posix [logfilename]
    if (argc > 2 && std::string(argv[2]) == "posix") {
        CONVERT_POSIX_TIME = true;
    }

    // Open log file if requested (third arg is filename)
    FILE* logfile = nullptr;
    if (argc > 3) {
        LOG_TO_FILE = true;
        std::string logname = argv[3];
        // Ensure .log extension
        if (logname.size() < 4 || logname.substr(logname.size() - 4) != ".log") {
            logname += ".log";
        }
        logfile = fopen(logname.c_str(), "w");
        if (!logfile) {
            perror("Log file");
            return 1;
        }
        std::cout << "Logging to " << logname << std::endl;
    }

    std::cout << "Capturing frames. Press Ctrl+C to stop.\n";
    double fps = 0;
    int frame_count = 0;
    auto last_time = std::chrono::steady_clock::now();

    // Main capture loop
    while (true) {
        // Dequeue a filled buffer (waits for next frame)
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("Retrieving Frame");
            break;
        }

        // Get timestamp: either POSIX epoch or V4L2 buffer timestamp
        double ts = 0.0;
        if (CONVERT_POSIX_TIME) {
            auto now = std::chrono::system_clock::now();
            ts = std::chrono::duration<double>(now.time_since_epoch()).count();
        } else {
            ts = buf.timestamp.tv_sec + buf.timestamp.tv_usec / 1e6;
        }

        // FPS calculation (updated every frame)
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_time).count();
        frame_count++;
        if (elapsed >= 1.0) {
            fps = frame_count / elapsed;
            frame_count = 0;
            last_time = now;
        }

        // Log every frame's timestamp and current FPS to file if enabled,
        // otherwise print to terminal only if timestamp mode is enabled
        if (LOG_TO_FILE && logfile) {
            fprintf(logfile, "Frame timestamp: %.6f | FPS: %.2f\n", ts, fps);
            fflush(logfile);
        } else if (TIMESTAMP_MODE) {
            printf("Frame timestamp: %.6f | FPS: %.2f\n", ts, fps);
        }

        // Requeue buffer for next capture
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("Requeue Buffer");
            break;
        }
    }

    // Cleanup resources
    if (logfile) fclose(logfile);
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(framebuf.start, framebuf.length);
    close(fd);
    return 0;
}
