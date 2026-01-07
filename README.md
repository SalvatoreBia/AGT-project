# Algorithmic Game Theory: Network security set

## Overview

This project implements multiple algorithmic approaches to solve the minimal network security set problem:

1. **Strategic Game Approach (file: algorithm.c)** - Players choose strategies to minimize individual costs
   - Best Response Dynamics (BRD)
   - Regret Matching (RM)
   - Fictitious Play (FP)

2. **Coalitional Game Approach (file: algorithm.c)** - Shapley value-based selection using Monte Carlo sampling

3. **Matching Market (file: min_cost_flow.c)** - Min-cost flow formulation with buyer-vendor matching

4. **VCG Auction (file: auction.c)** - Truthful auction mechanism for path routing

## Prerequisites

### All Platforms
- **GCC** or compatible C compiler
- **GLib 2.0** library
- **Make** build system

### macOS
```bash
# Using Homebrew
brew install glib pkg-config
```

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential libglib2.0-dev pkg-config
```

### Fedora/RHEL
```bash
sudo dnf install gcc glib2-devel pkgconfig
```

## Building

```bash
# Standard build
make

# Build with logging enabled
make LOG=1

# Clean build artifacts
make clean

# Build and run
make run
```

The compiled binary will be located at `build/main`.

## Usage

```bash
./build/main [options]
```

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-n <nodes>` | Number of nodes in the graph | 10000 |
| `-k <value>` | Degree parameter (varies by graph type) | 4 |
| `-t <type>` | Graph type (see below) | 0 |
| `-i <iterations>` | Maximum iterations | 10000 |
| `-a <algorithm>` | Algorithm selection (see below) | 3 |
| `-v <version>` | Shapley characteristic function version (1-3) | 3 |
| `-c <capacity>` | Capacity mode for matching market | 0 |
| `-h` | Show help message | - |

### Graph Types (`-t`)

| Value | Type | `-k` Meaning |
|-------|------|--------------|
| 0 | Regular | Node degree |
| 1 | Erdős-Rényi | Average degree |
| 2 | Barabási-Albert | Parameter m (edges per new node) |

### Algorithms (`-a`)

| Value | Algorithm |
|-------|-----------|
| 1 | Best Response Dynamics (BRD) |
| 2 | Regret Matching (RM) |
| 3 | Fictitious Play (FP) |
| 4 | Shapley Values (Monte Carlo) |

### Capacity Modes (`-c`)

| Value | Mode |
|-------|------|
| 0 | Infinite capacity |
| 1 | Limited capacity |
| 2 | Both modes |

## Examples

```bash
# Run Fictitious Play on a 1000-node regular graph with degree 6
./build/main -n 1000 -k 6 -t 0 -a 3

# Run Shapley values on an Erdős-Rényi graph
./build/main -n 500 -k 8 -t 1 -a 4 -v 3 -i 5000

# Run Best Response Dynamics on a Barabási-Albert graph with limited capacity
./build/main -n 2000 -k 3 -t 2 -a 1 -c 1

# Run all capacity modes (infinite + limited)
./build/main -n 1000 -a 3 -c 2
```

Logs are saved to `log_n<nodes>_k<param>_t<type>_a<algo>_c<cap>.log`.
