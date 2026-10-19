# Wavelet Analysis CLI — Implementation Plan

A C++26 command-line tool that applies Continuous Wavelet Transform (CWT) and
Haar Discrete Wavelet Transform (DWT) to source code / text files, surfacing
structural boundaries, complexity hotspots, and multi-resolution summaries.
Reference implementation (TypeScript/MCP): https://github.com/yogthos/wavescope-mcp

---

## Repository Layout (target state)

```
wavelet-analysis/
├── CMakeLists.txt
├── Makefile                    # cmake wrapper (make build/test/clean/run)
├── TODO.md
├── README.md
└── src/
    ├── main.cpp                # CLI entry point (CLI11)
    ├── util/
    │   └── aliases.h           # project-wide type/namespace aliases
    ├── lang/
    │   ├── lang_config.h       # LanguageConfig struct + registry
    │   └── lang_config.cpp
    ├── signal/
    │   ├── signal_gen.h        # text lines → float signal
    │   └── signal_gen.cpp
    ├── wavelet/
    │   ├── ricker.h            # Ricker kernel generation
    │   ├── ricker.cpp
    │   ├── cwt.h               # Continuous Wavelet Transform
    │   ├── cwt.cpp
    │   ├── haar.h              # Haar DWT (cascade)
    │   └── haar.cpp
    ├── analysis/
    │   ├── peaks.h             # peak detection + ridge collapse
    │   ├── peaks.cpp
    │   ├── bands.h             # multi-resolution band assembly
    │   ├── bands.cpp
    │   ├── heatmap.h           # complexity heatmap from Haar DWT
    │   └── heatmap.cpp
    ├── io/
    │   ├── file_reader.h       # read file into vector<string>
    │   ├── file_reader.cpp
    │   ├── output.h            # table / JSON / CSV formatters
    │   └── output.cpp
    └── tests/
        ├── CMakeLists.txt
        ├── test_signal.cpp
        ├── test_cwt.cpp
        ├── test_peaks.cpp
        └── test_haar.cpp
```

---

## Build System

| Command          | Effect                                              |
|------------------|-----------------------------------------------------|
| `make build`     | Configure (Debug) + compile                         |
| `make release`   | Configure (Release, -O3) + compile                  |
| `make test`      | Build with `BUILD_TESTS=ON` + run CTest             |
| `make clean`     | Remove `build/`                                     |
| `make compdb`    | Symlink `compile_commands.json` at root for clangd  |
| `make run ARGS=` | Run binary with arguments                           |

CLI11 is fetched automatically via CMake `FetchContent` if not found system-wide
(`apt install libcli11-dev` works as a faster local alternative).

---

## Code Style

All C++ follows the **format-cpp** skill rules (`.claude/skills/format-cpp/`).
Key conventions:
- 2-space indent, 80-char target, same-line `{`
- `struct` over `class`; `struct` members default-public
- `lower_snake_case` functions, `UpperCamelCase` types, `mUpperCamelCase` private fields
- `not`/`and`/`or`/`xor` instead of `!`/`&&`/`||`/`^`
- `string_view` for read-only string params; `optional<T>` instead of `T*` returns
- `#ifndef` header guards; angle-bracket includes everywhere
- Project namespace: `wa`; aliases in `<util/aliases.h>` (`str`, `strv`, `vec<T>`, `opt<T>`, …)

---

## Key Data Structures

```cpp
// Numeric signal derived from text (one float per line)
using Signal = vec<float>;

// CWT output: coefficients[scale_index][line_index]
struct WaveletCoefficients {
  vec<int>         scales;       // e.g. {1,2,4,8,16,32,64,128}
  vec<vec<double>> coefficients;
};

// A detected structural peak
struct Peak {
  int    position;    // line index (0-based)
  double coefficient; // CWT value at this position/scale
  int    scale;       // scale at which peak was detected
  str    label;       // inferred label ("class", "def", …) optional
};

// Multi-resolution context around a query line
struct Context {
  vec<s::pair<int, str>> fine;   // exact lines ±small_radius
  vec<Peak>              medium; // peaks at scales 1–16 within radius
  vec<Peak>              coarse; // peaks at scales 32–128 within radius
};

// Haar DWT result for a single level
struct HaarLevel {
  vec<double> approx;  // low-pass (scaling coefficients)
  vec<double> detail;  // high-pass (wavelet coefficients)
};

// Per-line complexity score
using Heatmap = vec<float>;   // range [0, 1], one entry per line
```

---

## Algorithm Reference

### 1. Signal Generation

Converts each text line to a float score in `[0, 2.0]` representing its
structural importance (class/function boundaries score high; blank lines score 0).

```
Constants:
  CODE_LINE_BASE   = 0.05f
  MAX_SCORE        = 2.0f
  MAX_INDENT_DEPTH = 8

Per-line pipeline:
  1. mask_strings(line)   — replace "…" / '…' content with spaces
  2. strip_block_comment  — track open/close via lang.block_comment delimiters
  3. strip_line_comment   — remove from lang.line_comment prefix to EOL
  4. tokenize             — split on non-word chars, lowercase tokens
  5. score = Σ lang.structural_keywords[token] for each token
  6. if score == 0 and masked line has non-whitespace: score = CODE_LINE_BASE
  7. if line is blank or pure comment: score = 0; next line
  8. depth = min(leading_spaces / 4, MAX_INDENT_DEPTH)
  9. score = score / (1 + depth * lang.indent_weight)   ← attenuation
 10. score = min(score, MAX_SCORE)
```

**Language keyword weights (representative values):**

| Keyword             | Weight |
|---------------------|--------|
| `class`, `struct`   | 2.0    |
| `def`, `function`   | 1.5    |
| `fn` (Rust)         | 1.5    |
| `func` (Go)         | 1.5    |
| `interface`, `trait`| 1.5    |
| `impl`, `module`    | 1.3    |
| `import`, `use`     | 0.8    |
| `if`, `for`, `while`| 0.3    |

`indent_weight` is 0.25 for most languages; 0.5 for Python (indentation is
semantically stronger there).

**Resources:**
- Reference implementation: `wavescope-mcp/src/signal.ts`

---

### 2. Ricker Wavelet (Mexican Hat)

```
ψ(t) = (1 − t²) · exp(−t²/2)

Kernel at scale a (discrete, length 2·half_width + 1):
  half_width = min(5 · a, N/2 − 1)
  k[j] = ψ((j − half_width) / a) / sqrt(a)   for j in [0, 2·half_width]
  k    -= mean(k)    // zero-mean: satisfies admissibility condition,
                     // prevents DC response when kernel is clipped at edges
```

Normalization by `1/√a` ensures L² energy is consistent across scales so
coefficients at different scales are directly comparable.

**Resources:**
- [Mexican hat wavelet — Wikipedia](https://en.wikipedia.org/wiki/Mexican_hat_wavelet)
- Mallat, *A Wavelet Tour of Signal Processing*, §2.4

---

### 3. Continuous Wavelet Transform (CWT)

```
W(a, b) = Σ_t  ψ_a(t − b) · signal[t]

Boundary condition — symmetric reflection (period = 2(N−1)):
  reflect_index(idx, N):
    p   = 2 * (N - 1)
    idx = abs(idx) % p
    if idx >= N: idx = p - idx
    return idx

Default scales: {1, 2, 4, 8, 16, 32, 64, 128}

For each scale a:
  kernel = make_kernel(a, N)
  hw     = kernel.size() / 2
  for each b in [0, N):
    W[a][b] = Σ_{j=0}^{kernel.size()-1}
                kernel[j] · signal[reflect_index(b + j − hw, N)]
```

Time complexity: O(S · N · K) where S = #scales, N = signal length,
K = kernel width ≈ 10·a. For large files, consider FFT-based convolution
(O(S · N · log N)).

**Resources:**
- [Continuous wavelet transform — Wikipedia](https://en.wikipedia.org/wiki/Continuous_wavelet_transform)
- Torrence & Compo (1998), "A practical guide to wavelet analysis", BAMS

---

### 4. Peak Detection

```
Input:  WaveletCoefficients, threshold, max_peaks=250, ridge_window=2
Output: vec<Peak>, sorted by |coefficient| descending

1. For each scale s, for each position i in [1, N−1):
     if |W[s][i]| >= threshold
     and |W[s][i]| >= |W[s][i−1]|    // ≥ left (handles plateaus)
     and |W[s][i]| >  |W[s][i+1]|:   // > right (rightmost of flat)
       emit Peak{i, W[s][i], s}

2. Sort all candidates by |coefficient| descending

3. Ridge collapse (greedy, O(P²) but P ≤ few thousand):
     keep = []
     for each peak p in sorted order:
       if no peak in keep is within ridge_window positions of p:
         keep.push(p)

4. Return keep[0 .. min(max_peaks, keep.size())]
```

**Resources:**
- [Local extremum — Wikipedia](https://en.wikipedia.org/wiki/Local_extremum)
- wavescope-mcp `detectPeaks` in `src/wavelet.ts`

---

### 5. Multi-Resolution Context

```
Input:  cwt, peaks, signal, lines, center_line, radius
Output: Context{fine, medium, coarse}

fine:
  window = min(radius / 10, 20)
  fine   = lines[max(0, center − window) .. min(N, center + window)]

medium (scales 1–16):
  medium = peaks filtered to |position − center| <= radius
           and scale in {1, 2, 4, 8, 16}
  sort by |position − center|

coarse (scales 32–128):
  coarse = peaks filtered to |position − center| <= radius
           and scale in {32, 64, 128}
  sort by |position − center|
```

---

### 6. Haar DWT

Standard Mallat cascade on a signal padded to the next power of 2.

```
Haar scaling filter: h = [1/√2,  1/√2]
Haar wavelet filter: g = [1/√2, −1/√2]

Single-level decomposition of x (length N, N even):
  approx[i] = (x[2i] + x[2i+1]) / √2   for i in [0, N/2)
  detail[i]  = (x[2i] − x[2i+1]) / √2  for i in [0, N/2)

Multi-level (L levels, pad signal to next power-of-2 length first):
  current = padded_signal
  for level = 1 to L:
    detail[level] = haar_single(current).detail
    current       = haar_single(current).approx
  approx_final = current
```

Number of levels L = floor(log2(N)); in practice cap at 8.

**Resources:**
- [Haar wavelet — Wikipedia](https://en.wikipedia.org/wiki/Haar_wavelet)
- Strang & Nguyen, *Wavelets and Filter Banks*, §3.1

---

### 7. Complexity Heatmap

Maps Haar detail coefficients back to per-line irregularity scores in [0, 1].

```
Input:  MultiLevelDWT (L levels of detail + final approx)
Output: Heatmap (vec<float>, length = original signal length)

Per-line score:
  irregularity[i] = Σ_{level=1}^{L}
                      |detail[level][i >> level]| / (2^level)

Normalize by max(irregularity) to bring into [0, 1].

Interpretation: high score = locally irregular structure (complex code);
                low score  = uniform, repetitive, or blank region.
```

---

## Implementation Steps

Steps are ordered by dependency. Complete each before starting the next.

---

### Step 1: Build System ✓

**Done.** CMakeLists.txt, Makefile, `src/main.cpp` skeleton, `src/util/aliases.h`.

Verify with: `make build`

**Checklist:**
- [x] `CMakeLists.txt` with FetchContent for CLI11
- [x] `Makefile` with `build` / `release` / `test` / `clean` / `compdb` / `run` targets
- [x] `src/main.cpp` CLI skeleton
- [x] `src/util/aliases.h`

---

### Step 2: Language Configuration

**File:** `src/lang/lang_config.h`, `src/lang/lang_config.cpp`

Define:
```cpp
struct LanguageConfig {
  str                  name;
  vec<str>             extensions;
  umap<str, float>     structural_keywords;
  float                indent_weight;
  str                    line_comment;   // e.g. "//"
  opt<s::pair<str, str>> block_comment; // nullopt = no block comments (Clojure)
};

opt<LanguageConfig> detect_language(strv filename);
const vec<LanguageConfig>& all_languages();
```

Implement configs for 9 languages:

| Language            | Extensions              | `indent_weight` | `line_comment` | `block_comment`  |
|---------------------|-------------------------|-----------------|----------------|------------------|
| C++                 | `.cpp .cc .cxx .h .hpp` | 0.25            | `//`           | `/* */`          |
| Python              | `.py`                   | 0.50            | `#`            | `""" """`        |
| TypeScript/JS       | `.ts .tsx .js .jsx`     | 0.25            | `//`           | `/* */`          |
| Go                  | `.go`                   | 0.25            | `//`           | `/* */`          |
| Rust                | `.rs`                   | 0.25            | `//`           | `/* */`          |
| Java                | `.java`                 | 0.25            | `//`           | `/* */`          |
| Scala               | `.scala`                | 0.25            | `//`           | `/* */`          |
| Clojure             | `.clj .cljs .cljc`      | 0.25            | `;`            | none             |
| Haskell             | `.hs .lhs`              | 0.40            | `--`           | `{- -}`          |

**Notable keyword weights per language:**

*Scala* — `class`/`object`/`trait`: 2.0; `def`/`type`: 1.5; `val`/`var`: 0.8;
`case`/`extends`/`with`: 0.5; `import`/`package`: 0.8

*Clojure* — `defn`/`defmacro`/`defprotocol`/`deftype`/`defrecord`: 2.0;
`ns`/`defmulti`/`defmethod`: 1.5; `def`: 1.0;
`extend-type`/`extend-protocol`: 1.3; `let`/`do`: 0.3

*Haskell* — `data`/`newtype`/`class`/`instance`: 2.0; `type`: 1.5;
`module`/`import`: 0.8; `where`: 0.5; `do`/`let`/`in`: 0.3

Haskell `indent_weight` is 0.40 (between Python and C-family) because layout
rules make indentation semantically significant but less strictly than Python.
Clojure has no block comment syntax — `#_` is a reader macro (form-level skip),
not a delimiter pair; treat it as a single-token comment mask rather than a
tracked open/close state.

Detection: match file extension, fall back to a generic config with
`CODE_LINE_BASE` only (no keyword weights).

**Acceptance:** `detect_language("foo.py")` returns Python config with
`indent_weight = 0.5`; `detect_language("foo.clj")` returns Clojure config
with `;` as line comment and empty block comment delimiters.

**Checklist:**
- [x] `lang_config.h` — `LanguageConfig` struct + function declarations
- [ ] `all_languages()` — C++ config
- [ ] `all_languages()` — Python config
- [ ] `all_languages()` — TypeScript/JS config
- [ ] `all_languages()` — Go config
- [ ] `all_languages()` — Rust config
- [ ] `all_languages()` — Java config
- [ ] `all_languages()` — Scala config
- [ ] `all_languages()` — Clojure config
- [ ] `all_languages()` — Haskell config
- [ ] `detect_language()` — extension extraction + registry lookup

---

### Step 3: File I/O

**File:** `src/io/file_reader.h`, `src/io/file_reader.cpp`

```cpp
struct ReadResult {
  vec<str> lines;
  str      error;   // non-empty on failure
};

ReadResult read_file(strv path);
```

Use `std::ifstream`. Return an error string (not an exception) so callers can
format it for the CLI without try/catch noise.

**Acceptance:** reads a 10 000-line file correctly; returns descriptive error
for missing or unreadable paths.

**Checklist:**
- [ ] `ReadResult` struct
- [ ] `read_file()` — `std::ifstream` read, line collection
- [ ] `read_file()` — descriptive error string for missing / unreadable paths

---

### Step 4: Signal Generation

**File:** `src/signal/signal_gen.h`, `src/signal/signal_gen.cpp`

```cpp
Signal compute_signal(const vec<str>& lines, const LanguageConfig& lang);
```

Implement the 10-step pipeline from the Algorithm Reference above.
String masking only needs to handle the common case (single-line strings);
multi-line strings are rare and can be skipped for now.

**Validation:**
- Blank line → 0.0
- `class Foo {` at depth 0 in C++ → ≈ 2.0 (capped)
- `  if (x) {` at depth 1 in C++ → ≈ 0.24 (0.3 / (1 + 1·0.25))
- Same `if` at depth 4 → ≈ 0.15

**Resources:** Algorithm Reference §1 above.

**Checklist:**
- [ ] `mask_strings()` — replace string literal contents with spaces
- [ ] Block comment state tracking
- [ ] Line comment stripping
- [ ] Tokenization — split on non-word chars, lowercase
- [ ] Keyword scoring — sum `structural_keywords` weights per token
- [ ] `CODE_LINE_BASE` fallback for non-blank, non-keyword lines
- [ ] Blank / pure-comment line zeroing
- [ ] Indent depth attenuation
- [ ] `MAX_SCORE` clamping
- [ ] `compute_signal()` — wire all 10 pipeline steps

---

### Step 5: Ricker Wavelet & CWT

**Files:** `src/wavelet/ricker.h/.cpp`, `src/wavelet/cwt.h/.cpp`

```cpp
// ricker.h
vec<double> make_kernel(int scale, int signal_length);

// cwt.h
enum struct Boundary : int { Reflect, Zero, };
WaveletCoefficients compute_cwt(
    const Signal& signal,
    const vec<int>& scales = {1, 2, 4, 8, 16, 32, 64, 128},
    Boundary boundary = Boundary::Reflect);
```

Implement direct convolution first (simplest correct approach). If profiling
shows it is too slow for files >5 000 lines, switch the inner loop to FFT-based
convolution using `std::fft` (C++26) or a small FFT library.

**Validation:**
- Kernel at scale 1 sums to ~0 (zero-mean property)
- Kernel energy: `Σ k[i]² ≈ 1` (L² normalization)
- On a unit-impulse signal `[0,…,0,1,0,…,0]`, the CWT output at position p
  should equal the kernel evaluated at the shift from p

**Resources:** Algorithm Reference §§2–3 above.

**Checklist:**
- [ ] `make_kernel()` — Ricker formula + zero-mean correction
- [ ] `reflect_index()` — symmetric boundary reflection helper
- [ ] `compute_cwt()` — direct convolution loop over all scales

---

### Step 6: Peak Detection

**File:** `src/analysis/peaks.h`, `src/analysis/peaks.cpp`

```cpp
struct Peak {
  int    position;
  double coefficient;
  int    scale;
  str    label;
};

vec<Peak> detect_peaks(
    const WaveletCoefficients& cwt,
    float  threshold   = 0.3f,
    int    max_peaks   = 250,
    int    ridge_window = 2);

// Snap peak positions to semantically meaningful lines (non-blank,
// non-comment) by scanning within ±snap_radius of the raw peak position.
vec<Peak> snap_peaks(
    vec<Peak> peaks,
    const vec<str>& lines,
    int snap_radius = 2);
```

**Validation:**
- On a signal with a single rectangular pulse, the peak should land at the
  pulse edge, not interior.
- Ridge collapse: two peaks at positions 5 and 6 from the same scale should
  collapse to the stronger one.

**Resources:** Algorithm Reference §4 above.

**Checklist:**
- [ ] `detect_peaks()` — local maximum detection with plateau rule
- [ ] `detect_peaks()` — sort candidates by `|coefficient|` descending
- [ ] `detect_peaks()` — ridge collapse (greedy deduplication within `ridge_window`)
- [ ] `snap_peaks()` — scan ±`snap_radius` for nearest semantic line

---

### Step 7: Multi-Resolution Context

**File:** `src/analysis/bands.h`, `src/analysis/bands.cpp`

```cpp
struct Context {
  vec<s::pair<int, str>> fine;   // {line_number, line_text}
  vec<Peak>              medium;
  vec<Peak>              coarse;
};

Context build_context(
    const WaveletCoefficients& cwt,
    const vec<Peak>& peaks,
    const vec<str>&  lines,
    int center,
    int radius = 300);
```

**Acceptance:** querying line 50 of a 200-line file with radius=100 returns
fine lines 45–55, medium peaks in [0,150], coarse peaks in [0,150].

**Checklist:**
- [ ] `build_context()` — fine window (lines ±`window` around center)
- [ ] `build_context()` — medium band: filter peaks at scales 1–16 within radius, sort by distance
- [ ] `build_context()` — coarse band: filter peaks at scales 32–128 within radius, sort by distance

---

### Step 8: Haar DWT

**File:** `src/wavelet/haar.h`, `src/wavelet/haar.cpp`

```cpp
struct HaarLevel {
  vec<double> approx;
  vec<double> detail;
};

struct MultiLevelDWT {
  vec<HaarLevel> levels;   // levels[0] = finest, levels[L-1] = coarsest
  vec<double>    final_approx;
};

// Pad signal to next power of 2, then decompose into min(max_levels,
// floor(log2(N))) levels.
MultiLevelDWT haar_dwt(const Signal& signal, int max_levels = 8);
```

**Validation:**
- Perfect reconstruction: `haar_idwt(haar_dwt(x)) ≈ x` (within float epsilon)
- Detail coefficients of a constant signal are all 0
- Energy preservation: `Σ x[i]² ≈ Σ_levels Σ d[i]² + Σ a[i]²`

**Resources:** Algorithm Reference §6 above; Strang & Nguyen §3.1.

**Checklist:**
- [ ] Pad signal to next power of 2
- [ ] `haar_single()` — one-level decomposition (`approx` + `detail`)
- [ ] `haar_dwt()` — multi-level cascade up to `max_levels`
- [ ] `haar_idwt()` — inverse transform for perfect-reconstruction validation

---

### Step 9: Complexity Heatmap

**File:** `src/analysis/heatmap.h`, `src/analysis/heatmap.cpp`

```cpp
Heatmap compute_heatmap(const Signal& signal);
```

Implement the irregularity scoring from Algorithm Reference §7.
Normalize the result to [0, 1] by dividing by `max(scores)` (skip
normalization if max is 0 to avoid divide-by-zero).

**Acceptance:**
- A 16-line signal `[0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0]` (single spike) should
  produce high irregularity scores around line 8 and low elsewhere.
- A constant non-zero signal should produce a zero heatmap.

**Checklist:**
- [ ] Per-line irregularity scoring: `Σ |detail[level][i >> level]| / 2^level`
- [ ] Normalize scores to [0, 1] (skip if max is 0)

---

### Step 10: Wire Up CLI Commands

**File:** `src/main.cpp` (extend the existing skeleton)

Add a `Commands` struct holding parsed options and a dispatch table:

| Subcommand     | Calls                                      |
|----------------|--------------------------------------------|
| `analyze`      | `read_file` → `compute_signal` → `compute_cwt` → `detect_peaks` → `snap_peaks` → output |
| `context`      | + `build_context` → output                 |
| `coefficients` | `compute_cwt` → output raw coefficients    |
| `heatmap`      | `compute_signal` → `haar_dwt` → `compute_heatmap` → output |
| `summary`      | `compute_cwt` → `detect_peaks` → print peak-annotated line slice |

Add a global `--json` flag to each subcommand that switches the output formatter.

**Checklist:**
- [ ] `analyze` subcommand
- [ ] `context` subcommand
- [ ] `coefficients` subcommand
- [ ] `heatmap` subcommand
- [ ] `summary` subcommand
- [ ] Global `--json` flag wired to each subcommand

---

### Step 11: Output Formatting

**File:** `src/io/output.h`, `src/io/output.cpp`

```cpp
enum struct OutputFormat : int { Table, Json, Csv, };

void print_peaks(
    const vec<Peak>&  peaks,
    const vec<str>&   lines,
    OutputFormat      fmt = OutputFormat::Table);

void print_coefficients(
    const WaveletCoefficients& cwt,
    int start, int end,
    OutputFormat fmt = OutputFormat::Table);

void print_context(const Context& ctx, OutputFormat fmt);
void print_heatmap(
    const Heatmap&  heatmap,
    const vec<str>& lines,
    OutputFormat    fmt);
```

Table format (default): fixed-width columns, human-readable.
JSON: hand-rolled (no external library needed; the structures are shallow).
CSV: for piping into analysis tools.

**Checklist:**
- [ ] `print_peaks()` — Table / JSON / CSV
- [ ] `print_coefficients()` — Table / JSON / CSV
- [ ] `print_context()` — Table / JSON / CSV
- [ ] `print_heatmap()` — Table / JSON / CSV

---

### Step 12: Tests

**Files:** `tests/CMakeLists.txt` + `tests/test_*.cpp`

Add Catch2 via FetchContent in `tests/CMakeLists.txt`:
```cmake
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)
```

Enable with: `make test` (sets `BUILD_TESTS=ON`).

**Test coverage targets:**

| File                  | What to test                                              |
|-----------------------|-----------------------------------------------------------|
| `test_signal.cpp`     | known-output lines, blank lines, keyword weights, depth   |
| `test_cwt.cpp`        | impulse response, kernel zero-mean, energy, reflection BC |
| `test_peaks.cpp`      | local maxima, plateau selection, ridge collapse           |
| `test_haar.cpp`       | perfect reconstruction, constant signal, energy           |
| `test_heatmap.cpp`    | spike signal, constant signal                             |
| `test_lang_config.cpp`| extension detection, fallback generic config              |

**Checklist:**
- [ ] `test_signal.cpp`
- [ ] `test_cwt.cpp`
- [ ] `test_peaks.cpp`
- [ ] `test_haar.cpp`
- [ ] `test_heatmap.cpp`
- [x] `test_lang_config.cpp`

---

## Resources

| Topic                     | Resource                                                          |
|---------------------------|-------------------------------------------------------------------|
| Ricker/Mexican hat wavelet | https://en.wikipedia.org/wiki/Mexican_hat_wavelet                |
| CWT theory                | Torrence & Compo 1998, BAMS (freely available PDF)               |
| Haar wavelet              | https://en.wikipedia.org/wiki/Haar_wavelet                       |
| Wavelet book              | Mallat, *A Wavelet Tour of Signal Processing* (3rd ed)           |
| CLI11 docs                | https://cliutils.gitlab.io/CLI11Tutorial/                        |
| C++26 reference           | https://en.cppreference.com/w/cpp/26                             |
| Reference implementation  | https://github.com/yogthos/wavescope-mcp                         |
| Format-cpp style rules    | `.claude/skills/format-cpp/SKILL.md`                             |
