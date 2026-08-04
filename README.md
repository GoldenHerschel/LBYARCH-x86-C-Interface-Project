# x86-to-C Interface Project: imgCvtGrayInttoFloat

Group members:
- Christian Gabriel Sanidad
- Adolfo Huruelo jr.

## What this does

Similar to csmodel vignette in python

Takes a grayscale image stored as uint8 pixels (0 to 255) and maps every pixel to a single
precision float between 0 and 1, using

```
f / i = 1 / 255   so   f = i * (1/255)
```

C handles the input, the memory allocation and the printing. The actual conversion is done
entirely in x86-64 assembly inside `imgCvtGrayInttoFloat`, using scalar SIMD registers (xmm)
and scalar SIMD floating point instructions (`cvtsi2ss`, `mulss`, `movss`, `xorps`).

## Files

| file | what it is |
|---|---|
| `main.c` | C driver, input, allocation, timing, correctness check, output |
| `imgcvt.asm` | the x86-64 asm function, works on both win64 and elf64 |
| `build.sh` | linux build |
| `build.bat` | windows build |
| `sample_input.txt` | the 3 x 4 example from the specs |

## How to build

Windows (nasm + mingw-w64 gcc):

```
nasm -f win64 imgcvt.asm -o imgcvt.obj
gcc -O2 main.c imgcvt.obj -o mp2.exe
```
## How to run

```
./mp2            runs the spec sample then the benchmark
./mp2 bench      benchmark only
./mp2 input      type in your own height, width and pixels
./mp2 input < sample_input.txt
```

## Correctness

The sample from the specs goes in and comes back out matching exactly:

```
input (uint8)
 64  89 114  84
140 166 191  84
216 242  38  84

output (single float, from asm)
0.25 0.35 0.45 0.33
0.55 0.65 0.75 0.33
0.85 0.95 0.15 0.33

correctness check: PASSED (0 of 12 pixels off)
```

Every run also checks itself. The C driver keeps a plain C reference version of the same
conversion, runs it on the same input, and compares pixel by pixel with a tolerance of 1e-6.
If even one pixel is off it prints FAILED plus the index and both values. All sizes pass.


## Execution time

Timer wraps the asm call only, nothing else. 30 runs per size, plus one untimed warmup run so
the first timed run is not eating the cold cache cost. Pixels are random values from 0 to 255
with a fixed seed so the runs repeat.

Machine: i7-14650hx 2200mhz, 32gb, Windows 11

| image size | pixels | avg (ms) | min (ms) | max (ms) | correct |
|---|---|---|---|---|---|
| 10 x 10 | 100 | 0.000037 | 0.000000 | 0.000100 | PASSED |
| 100 x 100 | 10,000 | 0.002807 | 0.002800 | 0.002900 | PASSED |
| 1000 x 1000 | 1,000,000 | 0.278227 | 0.264100 | 0.298000 | PASSED |

## Analysis

The runtime grows just about linearly with the number of pixels, which makes sense because the
assembly does the same small set of steps for every pixel and nothing else. Going from 10,000
pixels to 1,000,000 is 100x more work and it took about 99x longer, so the cost really is just
per pixel cost times the number of pixels. The 10 x 10 result is the exception, its minimum
time shows up as 0.000000 ms because the function finished faster than the timer could measure,
so the numbers at that size are basically noise and should not be read too closely. At
1000 x 1000 the program is moving 5 MB of data per call, and at that point the limit is how
fast memory can feed the CPU rather than how fast the math runs, since the float output takes
up four times the space of the uint8 input.
