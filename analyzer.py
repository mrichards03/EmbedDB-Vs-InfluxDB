import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from influxdb import InfluxDBClient

def load_data(file_path):
    """Load the CSV file into a pandas DataFrame and clean it."""
    df = pd.read_csv(file_path)
    df['timestamp'] = df['timestamp'].astype(int)
    df['latency'] = pd.to_numeric(df['latency'], errors='coerce')
    if 'kapacitor_time' in df.columns:
        df['kapacitor_time'] = df['kapacitor_time'].astype(int)
    if 'callback_time' in df.columns:
        df['callback_time'] = df['callback_time'].astype(int)
    return df

def analyze_performance(df):
    """Calculate and display performance metrics based only on INSERT events."""
    avg_insert_time = df['latency'].mean()
    print(f"Average Time: {avg_insert_time:.6f} ns")
    print(f"Max Time: {df['latency'].max()} ns at {df.loc[df['latency'].idxmax(), 'timestamp']} ns")
    print(f"Min Time: {df['latency'].min()} ns at {df.loc[df['latency'].idxmin(), 'timestamp']} ns")

    if('kapacitor_time' in df.columns):
        total_time = df["kapacitor_time"].max() - df["timestamp"].min()    
        print(f"Total Time: {total_time} ns")
    elif('callback_time' in df.columns):
        total_time = df["callback_time"].max() - df["timestamp"].min()
        print(f"Total Time: {total_time} ns")
    elif('timestamp' in df.columns):
        total_time = df["timestamp"].max() - df["timestamp"].min()    
        print(f"Total Time: {total_time} ns")
    print(f"Total Events: {len(df)}")
    return avg_insert_time


def plot_data_insertion(insert_df, callback_df = None):
    """Generate plots for insertion time trends and callback events."""
    plt.figure(figsize=(12, 8))    
    insert_df = insert_df.copy()
    insert_df.reset_index(drop=True, inplace=True)
    # Insertion time over time
    plt.subplot(2, 1, 1)
    plt.plot(insert_df.index, insert_df['latency'], label='Insertion Time (ns)', color='g', alpha=0.7)
    
    plt.xlabel('Index')
    plt.ylabel('Time (ns)')
    plt.legend()
    return plt

def prep_influx_data(client):    
    # Query alert data
    alert_query = 'SELECT * FROM "alerts"'
    alerts = client.query(alert_query).raw['series'][0]['values']
    alerts_last = pd.DataFrame(alerts, columns=['time', 'average', 'average_1', 'kapacitor_time']).tail(2000)

    # Convert string timestamps to datetime objects
    alerts_last['time'] = pd.to_datetime(alerts_last['time'])
    alerts_last['kapacitor_time'] = pd.to_datetime(alerts_last['kapacitor_time'])

    # Convert timestamps to integers (nanoseconds)
    alerts_last['timestamp'] = alerts_last['time'].astype(int)
    alerts_last['kapacitor_time'] = alerts_last['kapacitor_time'].astype(int)
    alerts_last['average'] = alerts_last['average'].astype(float)

    # duplicate_timestamps = alerts[alerts.duplicated(subset=['timestamp'], keep=False)]
    # if not duplicate_timestamps.empty:
    #     print("Duplicate timestamps found:")
    #     print(duplicate_timestamps)
    # else:
    #     print("No duplicate timestamps found.")

    # invalid_alerts = alerts[alerts['kapacitor_time'] < alerts['timestamp']]
    # print(invalid_alerts[['timestamp', 'kapacitor_time']])
    # Remove invalid alerts
    # alerts.loc[alerts['kapacitor_time'] < alerts['timestamp'], 'kapacitor_time'] = alerts['timestamp']
    # alerts = alerts[alerts['kapacitor_time'] >= alerts['timestamp']]
    # Sort by time for merge_asof
    alerts_last = alerts_last.sort_values('timestamp')

    # Calculate latency (kapacitor_time - time_data)
    alerts_last['latency'] = alerts_last['kapacitor_time'] - alerts_last['timestamp']    
    return alerts_last

def plot_latency_comparison(embeddb_insert_df_last, influx_data, avg_insert_time, advanced=False):
    """Plot Kapacitor latency vs Active Rule latency - average insert time."""
    
    # Ensure both datasets have the same length (truncate if necessary)
    min_length = min(len(embeddb_insert_df_last), len(influx_data))
    embeddb_insert_df_last = embeddb_insert_df_last.iloc[:min_length].copy()
    influx_data = influx_data.iloc[:min_length].copy()

    # Compute streaming query latency - average insert time
    streaming_query_latency_adjusted = embeddb_insert_df_last['latency'] - avg_insert_time
    # Adjust index for both DataFrames to start at 0
    embeddb_insert_df_last.reset_index(drop=True, inplace=True)
    influx_data.reset_index(drop=True, inplace=True)
    # Plot data
    plt.figure(figsize=(10, 6))
    plt.plot(embeddb_insert_df_last.index, streaming_query_latency_adjusted, label='Active Rule Latency', color='b', alpha=0.7)
    label = 'Advanced Query Latency' if advanced else 'Kapacitor Latency'
    plt.plot(influx_data.index, influx_data['latency'], label=label, color='r', alpha=0.7)

    plt.xlabel('Index')
    plt.ylabel('Latency (ns)')
    title = 'Advanced Query Latency vs Active Rule Latency' if advanced else 'Kapacitor Latency vs Active Rule Latency'
    plt.title(title)    
    plt.legend()
    plt.show()

def plot_average_temps(embeddb_df, influx_df, embeddb_advanced_df, kapacitor_df):
    embeddb_df = embeddb_df.copy()
    embeddb_advanced_df = embeddb_advanced_df.copy()
    influx_df = influx_df.copy()
    embeddb_df.reset_index(drop=True, inplace=True)
    influx_df.reset_index(drop=True, inplace=True)
    embeddb_advanced_df.reset_index(drop=True, inplace=True)
    kapacitor_df.reset_index(drop=True, inplace=True)
    plt.figure(figsize=(10, 6))
    plt.plot(embeddb_df.index, embeddb_df['temperature'], label='Active Rule Average Temperature', color='b', alpha=0.7)
    plt.plot(influx_df.index, influx_df['average'], label='Kapacitor Average Temperature', color='r', alpha=0.7)
    plt.plot(embeddb_advanced_df.index, embeddb_advanced_df['temperature'], label='Advanced Streaming Query Average Temperature', color='g', alpha=0.7)
    plt.plot(kapacitor_df.index, kapacitor_df['average'], label='Kapacitor Average Temperature From Callback', color='y', alpha=0.7)

    plt.xlabel('Index')
    plt.ylabel('Temperature (°C)')
    plt.title('Kapacitor Average Temperature vs Active Rule Average Temperature')
    plt.legend()
    plt.show()

def main():
    embeddb_file_path = "C:/Users/richa/OneDrive/Documents/influxdb/embeddb_perf.csv"
    embeddb_advanced_file_path = "C:/Users/richa/OneDrive/Documents/influxdb/embeddb_advanced_perf.csv"
    kapacitor_file_path = "C:/Users/richa/OneDrive/Documents/influxdb/alert_log.csv"
    embeddb_df = load_data(embeddb_file_path)
    embeddb_advanced_df = load_data(embeddb_advanced_file_path)
    kapacitor_df = load_data(kapacitor_file_path)

    embeddb_insert_df_first = embeddb_df[embeddb_df['event'] == 'INSERT'].copy().head(2000)
    embeddb_insert_df_last = embeddb_df[embeddb_df['event'] == 'INSERT'].copy().tail(2000)
    embeddb_callback_df = embeddb_df[embeddb_df['event'] == 'CALLBACK'].copy()

    embeddb_advanced_df_last = embeddb_advanced_df[embeddb_advanced_df['event'] == 'INSERT'].copy().tail(2000)
    embeddb_advanced_df_callback = embeddb_advanced_df[embeddb_advanced_df['event'] == 'CALLBACK'].copy()

    kapacitor_df_last = kapacitor_df.copy().tail(2000)

    print("\nInsert Time Without Streaming Query:")
    avg_insert_time = analyze_performance(embeddb_insert_df_first)  
    plt = plot_data_insertion(embeddb_insert_df_first)      
    plt.title('Insertion Time Trend Without Active Rule')

    print("\nInsert Time With Streaming Query:")
    analyze_performance(embeddb_insert_df_last)
    plt = plot_data_insertion(embeddb_insert_df_last)
    plt.title('Insertion Time Trend With Active Rule')

    print("\nInsert Time With Advanced Streaming Query:")
    analyze_performance(embeddb_advanced_df_last)
    plt = plot_data_insertion(embeddb_advanced_df_last)
    plt.title('Insertion Time Trend With Advanced Streaming Query')

    print("\nKapacitor Alert Latency with Callback:")
    analyze_performance(kapacitor_df_last)
    plt = plot_data_insertion(kapacitor_df_last)
    plt.title('Kapacitor Alert with Callback Latency Trend')


    
    # Initialize InfluxDB client
    client = InfluxDBClient(host='localhost', port=8086, database='temperature_db')
    
    influx_data = prep_influx_data(client)
    print("\nLatency for Kapacitor Alerts:")
    analyze_performance(influx_data)
    plt = plot_data_insertion(influx_data)
    plt.title('Kapacitor Alert Latency Trend')

    # Plot comparison
    plot_latency_comparison(embeddb_insert_df_last, influx_data, avg_insert_time)
    plot_latency_comparison(embeddb_insert_df_last, embeddb_advanced_df_last, avg_insert_time, True)
    plot_latency_comparison(embeddb_insert_df_last, kapacitor_df_last, avg_insert_time)


    plot_average_temps(embeddb_callback_df, influx_data, embeddb_advanced_df_callback, kapacitor_df_last)



if __name__ == "__main__":
    main()
