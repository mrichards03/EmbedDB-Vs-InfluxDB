#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096
#define timegm _mkgmtime 

#define CSV_FILE "C:\\Users\\richa\\OneDrive\\Documents\\influxdb\\alert_log.csv"
#define ERROR_LOG "C:\\Users\\richa\\OneDrive\\Documents\\influxdb\\error_log.txt"

// Global file handles for persistent access
static FILE *csv_file = NULL;
static FILE *error_file = NULL;

// Initialize file handles
void init_files() {
    if (!error_file) {
        error_file = fopen(ERROR_LOG, "a");
        if (!error_file) {
            fprintf(stderr, "Critical: Could not open error log file!\n");
            exit(1);
        }
    }
    
    if (!csv_file) {
        csv_file = fopen(CSV_FILE, "a");
        if (!csv_file) {
            fprintf(error_file, "Failed to open CSV file\n");
        }
    }
}

// Function to log errors to a file
void log_error(const char *message) {
    init_files();
    fprintf(error_file, "%s\n", message);
    fflush(error_file);
}

// Function to get the current time in nanoseconds
long long get_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1e9 + (long long)ts.tv_nsec;
}

// Function to write latency and avg_temp to a CSV file
void log_to_csv(long long latency_ns, double avg_temp, long long insert_ns, long long current_ns) {
    init_files();
    if (csv_file) {
        fprintf(csv_file, "%lld,%.6f,%lld,%lld\n", latency_ns, avg_temp, insert_ns, current_ns);
        fflush(csv_file);
    } else {
        log_error("Failed to write to CSV file");
    }
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

void process_alert(const char *buffer) {
    // Extract time and average value
    char *time_str = extract_first_number(buffer);
    char *avg_str = extract_second_number(buffer);

    if (!time_str || !avg_str) {
        log_error("Failed to extract time or average");
        return;
    }

    long long insert_ns = parse_time_ns(time_str);
    if (insert_ns == -1) {
        log_error("Failed to parse time string");
        return;
    }
    
    // Get current time in ns
    long long current_ns = get_nanoseconds();
    
    long long latency = current_ns - insert_ns;
    double avg_temp = atof(avg_str);

    // Log latency and average temp
    log_to_csv(latency, avg_temp, insert_ns, current_ns);
}

DWORD WINAPI process_alert_thread(LPVOID lpParam) {
    char *buffer = (char *)lpParam;
    process_alert(buffer);  // Your existing processing logic
    free(buffer);           // Free the copied buffer
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // Configure socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    init_files();
    while (1) {
        SOCKET new_socket = accept(server_fd, NULL, NULL);
        int yes = 1;
        setsockopt(new_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
        if (new_socket == INVALID_SOCKET) {
            log_error("Accept failed");
            continue;
        }
    
        // Read the HTTP request
        char *buffer = malloc(BUFFER_SIZE);
        int bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            free(buffer);
            closesocket(new_socket);
            continue;
        }
    
        // Send HTTP response immediately (don't wait for processing)
        const char *response = 
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"  // Force Kapacitor to reopen a connection
        "Content-Type: text/plain\r\n\r\n"
        "Alert received!";
        send(new_socket, response, strlen(response), 0);
        closesocket(new_socket);
    
        // Spawn a thread to process the alert
        HANDLE thread = CreateThread(
            NULL,
            0,
            process_alert_thread,
            (LPVOID)buffer,
            0,
            NULL
        );
        if (!thread) {
            log_error("Failed to create thread");
            free(buffer);
        } else {
            CloseHandle(thread);  // Thread will free its own buffer
        }
    }

    if (csv_file) fclose(csv_file);
    if (error_file) fclose(error_file);

    closesocket(server_fd);
    WSACleanup();
    return 0;
}