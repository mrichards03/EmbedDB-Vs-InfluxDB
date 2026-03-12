#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <curl/curl.h>
#include <windows.h>
#define INFLUXDB_URL "http://localhost:8086/write?db=temperature_db"
#define MEASUREMENT "temperature"
#define NUM_INSERTIONS 4000

uint64_t get_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1e9 + (uint64_t)ts.tv_nsec;
}

int8_t post_data(CURL *curl, float temperature, int i, FILE *logFile) {
    char payload[100];
    snprintf(payload, sizeof(payload), "temperature value=%f %llu\n", temperature, get_nanoseconds());

    curl_easy_setopt(curl, CURLOPT_URL, INFLUXDB_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(logFile, "Failed to insert data: %s\n", curl_easy_strerror(res));
    }
    return (res == CURLE_OK) ? 1 : 0;
}

int main() {
    FILE *logFile = fopen("error_log.txt", "w");
    if (!logFile) {
        fprintf(stderr, "Failed to open log file\n");
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(logFile, "Failed to initialize CURL\n");
        fclose(logFile);
        return 1;
    }

    srand(12345);  // Seed random number generator
    uint64_t startTime = get_nanoseconds();

    int successfulInsertions = 0;
    for (int i = 0; i < NUM_INSERTIONS; i++) {
        float temperature = 15 + (float)rand() / RAND_MAX * 15;
        int8_t success = post_data(curl, temperature, i, logFile);
        if (success) {
            successfulInsertions++;
        } else {
            fprintf(logFile, "Failed to insert record %d\n", i + 1);
        }
        Sleep(50);  // Sleep for 1 ms to avoid overwhelming the server
    }

    uint64_t endTime = get_nanoseconds();
    curl_easy_cleanup(curl);
    fclose(logFile);

    double totalTime = (double)(endTime - startTime) / 1e9;
    double throughput = successfulInsertions / totalTime;
    printf("Throughput: %.2f insertions/second\n", throughput);
    printf("Successful insertions: %d\n", successfulInsertions);
    return 0;
}