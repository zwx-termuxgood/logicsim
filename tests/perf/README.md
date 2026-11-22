# Standalone C++ performance harness

This project is deliberately **not** added to the application's CMake build.
It uses Qt Core only, and builds the selected tree's core/logic/engine libraries
using that tree's own CMake source lists. Thus the exact same benchmark source
can measure the baseline (which has no `tests/perf`) and the optimized tree.
All generated files should stay in external scratch directories.

## Build and run

From the current repository root (Qt >= 6.5, CMake and a C++17 compiler required):

```sh
ROOT="$PWD"
QT_PREFIX=/home/zwx/Tools/qt/Qt/6.11.1/gcc_64 # adjust for your installation
BASE_SOURCE="$PI_SCRATCH_DIR/perf-baseline"
BASE_BENCH="$PI_SCRATCH_DIR/perf-harness-base"
OPT_BENCH="$PI_SCRATCH_DIR/perf-harness-opt"

cmake -S "$ROOT/tests/perf" -B "$BASE_BENCH" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DLOGICSIM_SOURCE_DIR="$BASE_SOURCE"
cmake --build "$BASE_BENCH" --parallel 2
cmake -S "$ROOT/tests/perf" -B "$OPT_BENCH" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DLOGICSIM_SOURCE_DIR="$ROOT"
cmake --build "$OPT_BENCH" --parallel 2

QT_HASH_SEED=0 "$BASE_BENCH/logicsim_perf" 256 21 > "$PI_SCRATCH_DIR/perf-base.jsonl"
QT_HASH_SEED=0 "$OPT_BENCH/logicsim_perf" 256 21 > "$PI_SCRATCH_DIR/perf-opt.jsonl"
```

The existing application build `perf-base-build` is left untouched; the external
`perf-harness-base` project recompiles the same baseline sources in Release.
This avoids linking headers from one revision to static libraries from another.
Use the same compiler, Qt, optimization flags, workload size and sample count for
both builds. For serious comparisons stop background builds, alternate baseline
and optimized runs, and repeat on an idle machine (optionally pin both to the
same CPU with `taskset`). These numbers are not CI timing thresholds.

Output is JSON Lines: one metadata row followed by one row per workload with
`median_ms`, nearest-rank `p95_ms`, `samples`, and a SHA-256 semantic fingerprint.
The median is the average of the central pair for an even sample count. Two
warmups are excluded. `QT_HASH_SEED=0` is also enforced inside the executable
before constructing the Qt application or workload hashes. Invalid arguments
exit 2; failure to settle or inconsistent fingerprints across repetitions exits 1.

Compare fingerprints **before** interpreting speedups:

```sh
python3 - "$PI_SCRATCH_DIR/perf-base.jsonl" "$PI_SCRATCH_DIR/perf-opt.jsonl" <<'PY'
import json, sys
runs = []
for path in sys.argv[1:]:
    with open(path) as f:
        rows = [json.loads(line) for line in f]
    runs.append({r['workload']: r for r in rows if 'workload' in r})
a, b = runs
assert a.keys() == b.keys()
for name in a:
    assert a[name]['sha256'] == b[name]['sha256'], name + ': semantic mismatch'
    print(name, 'median speedup:', round(a[name]['median_ms'] / b[name]['median_ms'], 3))
PY
```

## Workloads and timing boundaries

* `event_build_settle`: build a source plus `size` 8-bit NOT gates in independent
  chains of length eight, with explicit Bezier bindings; drain to idle. Build and
  initial settlement are timed, engine allocation/destruction and observation
  are not.
* `event_toggle_16`: build and settle the same circuit **outside** timing; toggle
  its source between all ones and all zeros sixteen times, draining after each.
  The measured block includes triggers and propagation, not setup/observation.
* `iter_evaluate`: evaluate an untouched copy of that circuit with
  `IterEngine::evaluateContext`. Includes topology generation and map writes;
  context copy/setup and observation are excluded.
* `topology_virtual_and_dots`: `size` disjoint orthogonal T networks, each with
  overlapping and duplicated paths and source/sink ports (2*size components,
  4*size wires). Both virtual-wire generation and connection-dot queries are
  timed, including result assignment but excluding canonical serialization.

Fingerprints contain all observed input/output port values for each engine
node; event fingerprints additionally include the sorted dirty IDs and processed
event count. Topology fingerprints contain sorted virtual-wire and dot records.
Sorting removes irrelevant container order, not duplicate records. Serialization
and hashing are always outside the measured interval. Fingerprints are regression
signals, not a proof of correctness or exhaustive coverage: these workloads do
not cover hierarchical circuits, feedback, tri-state or every component type.
The toggle fingerprint observes the final settled state, not every intermediate
transition. No desktop QML/frame workload is included; this harness must not be
used to claim rendering or end-to-end UI improvements.

## Initial measurements

Local Release builds, Qt 6.11.1, size 256, 21 measured repetitions after two
warmups. Baseline source: `$PI_SCRATCH_DIR/perf-baseline`; optimized source:
current working tree. Both executable runs completed and **all four fingerprints
matched**. Shared host/background activity was not controlled, so treat these as
smoke measurements and repeat before making performance claims.

| Workload | Baseline median / p95 (ms) | Optimized median / p95 (ms) | Median ratio |
|---|---:|---:|---:|
| event_build_settle | 4.000 / 12.850 | 1.866 / 5.695 | 2.14x |
| event_toggle_16 | 3.822 / 5.851 | 3.531 / 4.580 | 1.08x |
| iter_evaluate | 8.106 / 37.150 | 5.723 / 9.153 | 1.42x |
| topology_virtual_and_dots | 39.109 / 63.936 | 10.180 / 11.064 | 3.84x |

Raw JSONL is in scratch `perf-base.jsonl` and `perf-opt.jsonl` (not versioned).
