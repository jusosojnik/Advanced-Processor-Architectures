# Advanced Processor Architectures — TAGE Branch Predictor

Course project completed at the **Universitat Politècnica de Catalunya (UPC)** during my Erasmus+ exchange in Barcelona.

The project explores dynamic branch prediction in modern processors using the **ChampSim** simulation framework.

This was a team project. **My contribution focused on the implementation, extension, testing, and evaluation of the branch-prediction component.**

## Overview

Modern CPUs use branch prediction to keep the instruction pipeline busy while the outcome of a branch is still unknown. Accurate predictions reduce pipeline stalls and can improve Instructions Per Cycle (IPC).

The goal of this project was to implement and evaluate a branch predictor based on **TAGE (Tagged Geometric History Length)**.

The predictor combines:

- a global branch-history register,
- multiple tagged prediction tables,
- different history lengths,
- tag-based matching,
- saturating prediction counters,
- alternate prediction logic,
- and a bimodal predictor as a fallback.

## How TAGE Works

For each branch, the instruction pointer and global branch history are used to compute an index and tag for multiple prediction tables.

The tagged tables are searched from the longest-history table toward shorter-history tables. The first matching entry becomes the primary prediction, while another match may be used as an alternate prediction.

If no suitable tagged entry is available, the predictor falls back to a bimodal prediction.

## Improvements Explored

Several extensions to the initial TAGE implementation were explored during the project:

- Branch-type-aware prediction
- Confidence tracking
- Alternate prediction selection
- Useful-bit aging
- Loop-bias tracking
- Return Address Stack (RAS)
- Global-history optimization
- Prime-spaced history lengths

These extensions were intended to improve prediction accuracy, reduce mispredictions, and make better use of different branch-history patterns.

## Evaluation

The predictor was evaluated using multiple ChampSim traces.

Example results comparing the initial and improved versions:

| Trace                  | Accuracy (initial) | Accuracy (improved) | MPKI (initial) | MPKI (improved) |
| ---------------------- | -----------------: | ------------------: | -------------: | --------------: |
| `429.mcf-217B`         |             97.22% |              98.11% |          3.157 |           2.152 |
| `445.gobmk-30B`        |             84.43% |              88.24% |          29.28 |           22.13 |
| `458.sjeng-283B`       |             91.22% |              92.16% |          19.88 |           17.76 |
| `600.perlbench_s-210B` |             96.12% |              96.79% |          6.128 |           5.075 |

The evaluation also tracked:

- Instructions Per Cycle (IPC)
- Branch-prediction accuracy
- Mispredictions Per Kilo Instructions (MPKI)
- Average Reorder Buffer occupancy at misprediction

## Repository Structure

```text
ChampSim/
├── branch/
│   ├── bimodal/
│   ├── gshare/
│   ├── hashed_perceptron/
│   ├── mybranch/
│   ├── perceptron/
│   ├── static_taken/
│   └── tage2/
├── config/
├── inc/
├── prefetcher/
├── src/
└── ...

PresentationBranchPredictor.pptx
```

The repository contains the ChampSim source code required for simulation, together with branch-predictor implementations, configurations, testing scripts, and evaluation results used during the project.

The repository also contains prefetcher components that were part of the wider team project. **My work focused on branch prediction rather than the prefetcher component.**

## Technologies and Concepts

- C++
- ChampSim
- Processor simulation
- Branch prediction
- TAGE
- Computer architecture
- Performance benchmarking

## Project Context

**Course:** Advanced Processor Architectures  
**University:** Universitat Politècnica de Catalunya (UPC), Barcelona  
**Year:** 2025

Team project by **Muhammad Ijaz, David Roche, and Juš Osojnik**.

My contribution focused on the **branch-prediction component**.
