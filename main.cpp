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
#include <fstream>
#include <map>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

// Buffer structure for memory-mapped frame data
struct buffer {
    void   *start;
    size_t length;
};

// Configuration structure
struct Config {
    bool print_to_console = false;
    bool use_posix_format = false;
    bool enable_file_logging = false;
    std::string log_filename = "";
    bool show_fps = true;
};

// Simple YAML parser for basic key-value pairs
Config load_config(const std::string& filename) {
    Config config;
    std::ifstream file(filename);
    std::string line;
    
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file " << filename << ", using defaults\n";
        return config;
    }
    
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Simple parsing for "key: value" format
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remove inline comments (everything after #)
        size_t comment_pos = value.find('#');
        if (comment_pos != std::string::npos) {
            value = value.substr(0, comment_pos);
            // Trim whitespace again after removing comment
            value.erase(value.find_last_not_of(" \t") + 1);
        }
        
        // Remove quotes if present
        if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        // Set config values
        if (key == "print_to_console") {
            config.print_to_console = (value == "true");
        } else if (key == "use_posix_format") {
            config.use_posix_format = (value == "true");
        } else if (key == "enable_file_logging") {
            config.enable_file_logging = (value == "true");
        } else if (key == "log_filename") {
            config.log_filename = value;
        } else if (key == "show_fps") {
            config.show_fps = (value == "true");
        }
    }
    
    return config;
}

// Flags for timestamp mode and logging (kept for compatibility)
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
    // Load configuration from config.yaml
    Config config = load_config("config.yaml");
    
    // Require at least one argument (video device)
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " /dev/videoX\n";
        std::cerr << "  /dev/videoX : Video device (required)\n";
        std::cerr << "\nAll other settings are configured in config.yaml\n";
        return 1;
    }

    // Select video device from command line argument
    const char* dev_name = argv[1];

    // All other parameters are read from config.yaml only
    // No command line override

    // Set legacy flags for compatibility
    TIMESTAMP_MODE = config.print_to_console;
    CONVERT_POSIX_TIME = config.use_posix_format;
    LOG_TO_FILE = config.enable_file_logging && !config.log_filename.empty();

    // Open video device
    int fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        perror("Cannot open video device");
        return 1;
    }
    
    // Check device capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("Query device capabilities");
        close(fd);
        return 1;
    }
    
    std::cout << "Device: " << dev_name << std::endl;
    std::cout << "Driver: " << cap.driver << std::endl;
    std::cout << "Card: " << cap.card << std::endl;
    
    // Check current format
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        perror("Get format");
        close(fd);
        return 1;
    }
    
    std::cout << "Format: " << fmt.fmt.pix.width << "x" << fmt.fmt.pix.height << std::endl;


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

    // Open log file if enabled in config
    FILE* logfile = nullptr;
    if (LOG_TO_FILE && !config.log_filename.empty()) {
        logfile = fopen(config.log_filename.c_str(), "w");
        if (!logfile) {
            perror("Log file");
            return 1;
        }
        std::cout << "Logging to " << config.log_filename << std::endl;
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
            
            // Debug: Check if V4L2 timestamp is valid
            if (buf.timestamp.tv_sec == 0 && buf.timestamp.tv_usec == 0) {
                std::cerr << "Warning: V4L2 timestamp is zero. Driver may not support timestamping." << std::endl;
                std::cerr << "Using system time instead..." << std::endl;
                auto now = std::chrono::steady_clock::now();
                ts = std::chrono::duration<double>(now.time_since_epoch()).count();
            }
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
