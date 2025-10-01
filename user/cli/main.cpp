
#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <thread>
#include <chrono>
#include <string>

/**
 * @brief Temperature sample structure matching kernel layout.
 */
struct simtemp_sample {
    uint64_t timestamp_ns; ///< Nanoseconds since epoch
    int32_t temp_mC;       ///< Temperature in milli-degrees Celsius
    uint32_t flags;        ///< Event flags (bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED)
} __attribute__((packed));


/* --- Sysfs Helpers --- */ 
namespace sysfs {
    const std::string base = "/sys/class/simtemp_class/simtemp/";

    /**
     * @brief Read the current sampling period from sysfs.
     * @return Sampling period in milliseconds, or 100 if not found.
     */
    unsigned int get_sampling_period_ms() {
        std::ifstream f(base + "sampling_ms");
        unsigned int ms = 100;
        if (f.is_open()) {
            f >> ms;
        }
        return ms;
    }

    /**
     * @brief Read the current threshold from sysfs.
     * @return Threshold in milli-degrees Celsius, or 45000 if not found.
     */
    int get_threshold_mC() {
        std::ifstream f(base + "threshold_mC");
        int val = 45000;
        if (f.is_open()) {
            f >> val;
        }
        return val;
    }

    /**
     * @brief Read the current mode from sysfs.
     * @return Mode string, or "normal" if not found.
     */
    std::string get_mode() {
        std::ifstream f(base + "mode");
        std::string m = "normal";
        if (f.is_open()) {
            f >> m;
        }
        return m;
    }
}

/**
 * @brief Convert nanoseconds since epoch to ISO 8601 formatted string.
 * @param[in] ns Nanoseconds since epoch.
 * @return ISO 8601 formatted time string.
 */
std::string format_iso8601(uint64_t ns) {
    time_t sec = ns / 1000000000ULL;
    uint32_t ms = (ns / 1000000ULL) % 1000;
    struct tm tm;
    char buf[32];
    char out[40];
    gmtime_r(&sec, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(out, sizeof(out), "%s.%03uZ", buf, ms);
    return std::string(out);
}

/**
 * @brief Format a simtemp_sample as a human-readable string.
 * @param[in] sample The temperature sample to format.
 * @return Formatted string for CLI output.
 */
std::string format_sample(const simtemp_sample& sample) {
    std::ostringstream oss;
    oss << format_iso8601(sample.timestamp_ns)
        << " temp=" << std::fixed << std::setprecision(1)
        << (sample.temp_mC / 1000.0)
        << "C alert=" << ((sample.flags & 0x2) ? 1 : 0);
    return oss.str();
}

/**
 * @brief Encapsulates /dev/simtemp device I/O.
 */
class SimTempDevice {
private:
    int fd_; ///< File descriptor for /dev/simtemp
    std::string devpath_;
public:
    /**
     * @brief Construct a SimTempDevice object.
     * @param[in] dev Device path (default: /dev/simtemp)
     */
    SimTempDevice(const std::string& dev = "/dev/simtemp") : fd_(-1), devpath_(dev) {}

    /**
     * @brief Destructor. Closes device if open.
     */
    ~SimTempDevice() { if (fd_ >= 0) close(fd_); }

    /**
     * @brief Open the device file for reading.
     * @return true on success, false on failure.
     */
    bool open_device() {
        fd_ = open(devpath_.c_str(), O_RDONLY);
        if (fd_ < 0) {
            perror("open");
            return false;
        }
        return true;
    }

    /**
     * @brief Read a temperature sample, waiting up to timeout_ms.
     * @param[out] sample Filled with sample data on success.
     * @param[in] timeout_ms Timeout in milliseconds for select().
     * @return true if a sample was read, false on timeout or error.
     */
    bool read_sample(simtemp_sample& sample, unsigned int timeout_ms) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd_+1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
            return false;
        }
        if (FD_ISSET(fd_, &rfds)) {
            ssize_t n = read(fd_, &sample, sizeof(sample));
            if (n == sizeof(sample)) {
                return true;
            } else if (n == 0) {
                /* EOF reached */
                return false;
            } else {
                perror("read");
                return false;
            }
        }
        return false;
    }
};

/**
 * @brief Monitor mode: continuously read and print temperature samples.
 */
void run_monitor_mode() {
    SimTempDevice dev;
    if (!dev.open_device()) {
        std::cerr << "Failed to open /dev/simtemp" << std::endl;
        return;
    } 
        
    while (true) {
        unsigned int period = sysfs::get_sampling_period_ms();
        simtemp_sample sample;
        if (dev.read_sample(sample, period)) {
            std::cout << format_sample(sample) << std::endl;
        } else {
            // Timeout or error: re-check config, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

/**
 * @brief Main CLI entrypoint. Parses arguments and dispatches modes.
 * @param[in] argc Argument count
 * @param[in] argv Argument vector
 * @return Exit code
 */
int main(int argc, char* argv[]) {
    /* TODO: parse args for future modes (test, config, etc) */
    run_monitor_mode();
    return 0;
}
