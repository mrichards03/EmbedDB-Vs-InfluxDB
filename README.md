# EmbedDB vs InfluxDB (Chapter 4 Reproduction Guide)

This README documents how to reproduce the Chapter 4 experiments comparing:

- EmbedDB Active Rules
- EmbedDB Advanced Streaming Query path
- InfluxDB + Kapacitor alert processing
- Cross-system analysis/plots in `analyzer.py`

It is based on the scripts and data formats currently present in this workspace.

## 1) What was actually used

### Core files used for the experiments

- `insert_fake_data.c`  
	Inserts 4000 temperature points into InfluxDB (`temperature_db`, measurement `temperature`).
- `temperature_alert.tick`  
	Kapacitor TICKscript writing alert output into InfluxDB measurement `alerts`.
- `temp_alert_listen.tick`  
	Kapacitor TICKscript that calls local listener executable and writes callback latency CSV.
- `alert_listener.c`
	Consumes Kapacitor alert payload and writes `alert_log.csv` format.
- `EmbedDB/src/benchmarks/activeRulesBenchmark.h`  
	Emits EmbedDB benchmark CSV in the same schema expected by `analyzer.py`.
- `EmbedDB/src/desktopMain.c`  
	Entrypoint used to select `WHICH_PROGRAM == 4` for `activeRulesBenchmark()`.
- `analyzer.py`  
	Produces final metrics and plots used in analysis.

### Files present but not used in the final thesis write up

- `insert_influx.py` (uses DB `benchmark`, 2000 inserts, prints only)
- `insert_fake_data_batch.c`
- `tem_alert_batch.tick`
- `alert_listener_http.c`

These are other experiments like batch insert benchmarks and listeners for kapacitor post() instead of executible for kapacitor.

## 2) Prerequisites

- Change hardcoded paths in code to match yours
- InfluxDB 1.x running on `localhost:8086`
- Kapacitor running and connected to InfluxDB 1.x
- Python 3.10+ with packages:
	- `pandas`
	- `matplotlib`
	- `numpy`
	- `influxdb`

Install Python deps:

```powershell
pip install pandas matplotlib numpy influxdb
```

## 3) Build executables

From `influxdb/`:

```powershell
gcc -O2 -o alert_listener.exe alert_listener.c
gcc -O2 -o insert_fake_data.exe insert_fake_data.c -lcurl
```

From `EmbedDB/` (desktop benchmark build):

`WHICH_PROGRAM=4` runs `activeRulesBenchmark()` from `src/benchmarks/activeRulesBenchmark.h`.

## 4) Run InfluxDB + Kapacitor experiments

### A) Task for alerts stored in Influx (`alerts` measurement)

```powershell
kapacitor define temp_alert_db -type stream -tick temperature_alert.tick -dbrp temperature_db.autogen
kapacitor enable temp_alert_db
```

This populates measurement `alerts` and is consumed in `analyzer.py` via `prep_influx_data()`.

### B) Task for callback latency CSV (`alert_log.csv`)

```powershell
kapacitor define temp_alert_callback -type stream -tick temp_alert_listen.tick -dbrp temperature_db.autogen
kapacitor enable temp_alert_callback
```

This uses `.exec('.../alert_listener')` and writes callback latencies to `alert_log.csv`.

### C) Run data insertion workload

From `influxdb/`:

```powershell
./insert_fake_data.exe
```

## 5) Run EmbedDB benchmarks

From `EmbedDB/`:

```powershell
make build CFLAGS="-DWHICH_PROGRAM=4"
```

This runs the active-rules benchmark and writes the output CSV (currently `embeddb_perf_new.csv` in the benchmark source).

### Producing both EmbedDB CSV variants used in analysis

`analyzer.py` expects two files:

- `embeddb_perf.csv` (active-rule path)
- `embeddb_advanced_perf.csv` (advanced streaming query path)

In the current benchmark source (`activeRulesBenchmark.h`), the advanced logging path is present but commented (`advancedPerfLog`, `GetAvgLocal(...)`).

To fully reproduce both Chapter 4 EmbedDB curves, run two passes:

1. **Active-rule pass**: use current default path and save output as `embeddb_perf.csv`.
2. **Advanced-query pass**: enable the commented advanced logging/query lines and save output as `embeddb_advanced_perf.csv`.

## 6) Run analysis

From `influxdb/`:

```powershell
python analyzer.py
```

`analyzer.py` uses:

- `embeddb_perf.csv`
- `embeddb_advanced_perf.csv`
- `alert_log.csv`
- live Influx query from measurement `alerts` in database `temperature_db`

It computes latency summaries and plots:

- Insert latency trends (EmbedDB variants)
- Kapacitor callback latency trend (`alert_log.csv`)
- Kapacitor measurement latency trend (`alerts`)
- Cross-system latency comparisons
- Average temperature comparisons

## 7) Expected output files

- `embeddb_perf.csv` (or `_new` then renamed)
- `embeddb_advanced_perf.csv` (or `_new` then renamed)
- `alert_log.csv`

All should contain nanosecond-scale timestamps/latencies and be non-empty before running final analysis.

