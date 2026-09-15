# Scan Chain Simulator

Sequential circuits are a pain to test because you can't directly control or observe flip-flop states from the primary I/Os. Scan insertion fixes this — you chain all the DFFs together into a shift register so you can load any state you want (shift-in) and read out what got captured (shift-out).

Built this to understand the full-scan flow before touching any commercial tools. Used ISCAS-89 `.bench` format for the netlists since that's the standard for sequential benchmark circuits.

## The idea

With full scan, every FF state becomes:
- **Controllable** — shift your test state in serially before the capture cycle
- **Observable** — shift out whatever the FFs captured after the cycle

So the circuit is effectively combinational between:
- **Pseudo-PIs** = primary inputs + FF Q-outputs
- **Pseudo-POs** = primary outputs + FF D-inputs

That's the whole point. Once you see it that way, applying random ATPG to a sequential circuit is just combinational ATPG on the pseudo-PI/PO space.

## Test flow

```
generate PI pattern + FF pattern
        ↓
   shift-in FF states
        ↓
   apply PI values + one capture cycle
        ↓
   read POs + shift-out FF D-inputs
        ↓
   compare against fault-free — any diff = fault detected
```

Fault injection is done by forcing the target gate's output to the stuck value during the capture simulation.

## Build

```bash
mkdir build && cd build
cmake .. && make
```

## Usage

```
./scan-simulator <circuit.bench> [-n num_patterns] [-seed value] [-o output_file]
```

| Flag | Default | Description |
|------|---------|-------------|
| `-n` | 1024 | number of scan patterns |
| `-seed` | 0xACE1 | LFSR seed |
| `-o` | — | write per-pattern CSV coverage data |

## Benchmark format

```
INPUT(name)
OUTPUT(name)
gate = AND(in1, in2)
ff   = DFF(d_input)
```

Supported: `AND`, `OR`, `NAND`, `NOR`, `NOT`, `BUFF`, `XOR`, `XNOR`, `DFF`

## Examples

```bash
./scan-simulator ../benchmarks/s27.bench
./scan-simulator ../benchmarks/s27.bench -n 2000 -o coverage.csv
./scan-simulator ../benchmarks/s_simple.bench -n 500
```

## Output

```
============================================
      Scan Chain Simulation Results
============================================
  Circuit:            s27
  Primary Inputs:     4
  Primary Outputs:    1
  Flip-Flops:         3
  Scan Chain Length:  3
  Pseudo-PIs:         7
  Pseudo-POs:         4
  Total Gates:        14
  Patterns Applied:   1000
  Total Faults:       28
  Detected Faults:    28
  Fault Coverage:     100.00%
============================================
```

## Results

| Circuit  | FFs | Pseudo-PIs | Patterns | Fault Coverage |
|----------|-----|------------|----------|----------------|
| s27      | 3   | 7          | 1000     | 100.00%        |
| s_simple | 3   | 6          | 1000     | 100.00%        |
