from influxdb import InfluxDBClient
import time
import random

NUM_INSERTIONS = 2000
DB_NAME = "benchmark"

client = InfluxDBClient(host='localhost', port=8086)
client.create_database(DB_NAME)
client.switch_database(DB_NAME)

# Insert without Kapacitor query first
for _ in range(NUM_INSERTIONS):
    timestamp = int(time.time() * 1e9)  # Nanosecond precision
    temperature = 15 + random.random() * 15  # 15°C to 30°C

    json_body = [{
        "measurement": "temperature",
        "tags": {"sensor": "benchmark"},
        "fields": {"value": temperature},
        "time": timestamp
    }]

    start = time.perf_counter_ns()
    client.write_points(json_body)
    end = time.perf_counter_ns()
    
    insert_time = end - start  # In nanoseconds
    print(f"{timestamp}, INSERT, {temperature}, {insert_time}")

client.close()
