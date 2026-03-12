#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define timegm _mkgmtime 

#define CSV_FILE "C:\\Users\\richa\\OneDrive\\Documents\\influxdb\\alert_log.csv"
#define ERROR_LOG "C:\\Users\\richa\\OneDrive\\Documents\\influxdb\\error_log.txt"

// Function to log errors to a file
void log_error(const char *message) {
    FILE *file = fopen(ERROR_LOG, "a");
    if (file) {
        fprintf(file, "%s\n", message);
        fclose(file);
    } else {
        fprintf(stderr, "Failed to open error log\n");
    }
}

// Function to get the current time in nanoseconds
long long get_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1e9 + (long long)ts.tv_nsec;
}

// Function to write latency and avg_temp to a CSV file
void log_to_csv(long long latency_ns, double avg_temp) {
    FILE *file = fopen(CSV_FILE, "a");
    if (!file) {
        log_error("Failed to open CSV file");
        return;
    }

    fprintf(file, "%lld,%.6f\n", latency_ns, avg_temp);
    fclose(file);
}

// Function to extract the first number from a given JSON-like string
char *extract_first_number(const char *json) {
    static char value[256];
    char *pos = strstr(json, "[["); // Find first "[["
    if (!pos) return NULL;

    pos+=2; // Move past '['
    while (*pos == ' ' || *pos == '"') pos++; // Skip spaces and quotes

    char *end = strchr(pos, ','); // Find the comma that separates values
    if (!end) return NULL;

    size_t len = end - pos;
    strncpy(value, pos, len);
    value[len] = '\0';

    return value;
}

// Function to extract the second number (temperature average) from the values section
char *extract_second_number(const char *json) {
    static char value[256];
    char *pos = strstr(json, "[["); // Find first "[["
    if (!pos) return NULL;

    pos = strchr(pos + 2, ','); // Find the comma after the first number
    if (!pos) return NULL;

    pos++; // Move past ','
    while (*pos == ' ' || *pos == '"') pos++; // Skip spaces and quotes

    char *end = strchr(pos, ']'); // Find the closing bracket
    if (!end) return NULL;

    size_t len = end - pos;
    strncpy(value, pos, len);
    value[len] = '\0';

    return value;
}

// Convert ISO 8601 timestamp to nanoseconds
long long parse_time_ns(const char *time_str) {
    struct tm t = {0};
    long long ns = 0;
    char frac_str[10] = {0}; // Buffer for fractional part
    
    // Parse with sscanf (expecting 9-digit nanoseconds)
    int parsed = sscanf(time_str, "%d-%d-%dT%d:%d:%d.%9[0-9]Z",
                        &t.tm_year, &t.tm_mon, &t.tm_mday,
                        &t.tm_hour, &t.tm_min, &t.tm_sec, frac_str);
    
    if (parsed < 7) { // Check if parsing failed
        return -1; // Error handling
    }

    // Ensure fractional part is 9 digits (pad with zeros if needed)
    size_t frac_len = strlen(frac_str);
    if (frac_len < 9) {
        memset(frac_str + frac_len, '0', 9 - frac_len);
    }
    ns = strtoll(frac_str, NULL, 10);

    // Adjust tm struct (required for timegm/mktime)
    t.tm_year -= 1900;
    t.tm_mon -= 1;

    // Convert to UTC epoch seconds (using timegm or fallback)
    time_t epoch_seconds;
    #ifdef _WIN32
        _putenv("TZ=UTC"); // Windows workaround
        tzset();
        epoch_seconds = mktime(&t);
        _putenv("TZ=");
        tzset();
    #else
        epoch_seconds = timegm(&t); // Linux/macOS
    #endif

    return (long long)epoch_seconds * 1000000000LL + ns;
}

int main() {
    FILE *logFile = fopen(ERROR_LOG, "a");
    if (!logFile) {
        fprintf(stderr, "Critical: Could not open error log file!\n");
        return 1;
    }

    // Read JSON input
    char buffer[8192];
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        fprintf(logFile, "Failed to read JSON input\n");
        fclose(logFile);
        return 1;
    }
    //fprintf(logFile, "Received JSON: %s\n", buffer);

    // Extract time and average value
    char *time_str = extract_first_number(buffer);
    char *avg_str = extract_second_number(buffer);

    if (!time_str || !avg_str) {
        fprintf(logFile, "Failed to extract time or average. time: %s, avg: %s\n", time_str, avg_str);
        fclose(logFile);
        return 1;
    }

    long long insert_ns = parse_time_ns(time_str);
    
    // Get current time in ns
    long long current_ns = get_nanoseconds();
    
    long long latency = current_ns - insert_ns;
    double avg_temp = atof(avg_str);

    // Log latency and average temp
    FILE *csvFile = fopen(CSV_FILE, "a");
    if (csvFile) {
        fprintf(csvFile, "%lld,%.6f,%lld,%lld\n", latency, avg_temp, insert_ns, current_ns);
        fclose(csvFile);
    } else {
        fprintf(logFile, "Failed to open CSV file\n");
    }

    fclose(logFile);
    return 0;
}