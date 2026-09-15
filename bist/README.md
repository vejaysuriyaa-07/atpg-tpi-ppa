# BIST Simulator

Built this to understand the LFSR/MISR architecture that shows up in pretty much every logic BIST implementation. The idea is simple — use an LFSR to generate pseudo-random test patterns, run them through the circuit, compress the output responses into a signature via a MISR, and track how many stuck-at faults you catch along the way.

Tested on ISCAS-85 benchmarks. Getting above 95% on c432 and c880 was the goal — ended up at 98.84% and 97.50% with 2000 patterns.

## How it works

**LFSR** — Galois-configuration shift register. Uses standard primitive polynomials (looked these up — there's a well-known table for widths 4, 8, 16, 32). For circuits with more primary inputs than the LFSR width, it just clocks the LFSR multiple times per pattern to fill all the inputs. This is called phase shifting.

**MISR** — Same hardware structure as the LFSR but instead of just shifting, it XORs the circuit's output bits into the register at each clock. The final state after all patterns is the signature. If the circuit has a fault, the signature will (almost certainly) differ from the golden value.

**Fault simulation** — For each test pattern, any fault not yet detected gets re-simulated with the fault injected (just force the gate output to the stuck value). If any PO differs from the fault-free run, the fault is marked detected. Keeps going until all patterns are done.

## Build

```bash
mkdir build && cd build
cmake .. && make
```

## Usage

```
./bist-simulator <circuit.ckt> [-n num_patterns] [-seed value] [-misr width] [-o output_file]
```

| Flag | Default | Description |
|------|---------|-------------|
| `-n` | 1024 | number of patterns to run |
| `-seed` | 0xACE1 | LFSR starting seed |
| `-misr` | 32 | MISR register width |
| `-o` | — | dump per-pattern CSV to a file |

## Examples

```bash
./bist-simulator ../../ckts/c17.ckt
./bist-simulator ../../ckts/c432.ckt -n 2000
./bist-simulator ../../ckts/c880.ckt -n 2000 -o coverage.csv
```

## Output

```
============================================
         BIST Simulation Results
============================================
  Circuit:            c432
  Primary Inputs:     36
  Primary Outputs:    7
  Total Gates:        432
  LFSR Width:         32
  MISR Width:         32
  Patterns Applied:   2000
  Total Faults:       864
  Detected Faults:    854
  Fault Coverage:     98.84%
  MISR Signature:     0x84887867
============================================
```

## Results

| Circuit | Patterns | Fault Coverage |
|---------|----------|----------------|
| c17     | 1024     | 100.00%        |
| c432    | 2000     | 98.84%         |
| c880    | 2000     | 97.50%         |
