#!/bin/bash

# Path to ChampSim binary
CHAMPSIM_BIN="./bin/champsim"

# Path to the traces folder
TRACE_DIR="/Users/jusosojnik/Desktop/APA/traces"

# Warmup and simulation instructions
WARMUP=200000000
SIM=500000000

# Output directory for logs
LOG_DIR="./branch_test_logs_2"
mkdir -p "$LOG_DIR"

# Final summary output
RESULTS_FILE="branch_accuracy_summary_2.txt"
echo "Trace File | Branch Prediction Accuracy" > "$RESULTS_FILE"
echo "---------------------------------------" >> "$RESULTS_FILE"

# List of traces to test
TRACE_LIST=(
  "445.gobmk-30B.champsimtrace.xz"
  "403.gcc-48B.champsimtrace.xz"
  "456.hmmer-191B.champsimtrace.xz"
  "458.sjeng-283B.champsimtrace.xz"
  # "410.bwaves-2097B.champsimtrace.xz"
  # "470.lbm-1274B.champsimtrace.xz"
  "429.mcf-217B.champsimtrace.xz"
  # "483.xalancbmk-736B.champsimtrace.xz"
  "600.perlbench_s-210B.champsimtrace.xz"
)

# Run simulation for each trace
for TRACE in "${TRACE_LIST[@]}"; do
  TRACE_PATH="./$TRACE"
  LOG_FILE="$LOG_DIR/${TRACE%.champsimtrace.xz}.log"

  echo "Running simulation for $TRACE..."
  $CHAMPSIM_BIN --warmup-instructions $WARMUP --simulation-instructions $SIM "$TRACE_DIR/$TRACE" > "$LOG_FILE"

  # Extract accuracy and append to results file
  ACCURACY=$(grep "Branch Prediction Accuracy" "$LOG_FILE" | grep -o '[0-9.]*%')
  echo "$TRACE | $ACCURACY" >> "$RESULTS_FILE"
done

echo "All simulations completed. Summary saved to $RESULTS_FILE"
