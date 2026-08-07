# x86-to-C Interface Project: imgCvtGrayInttoFloat

Group members:
- Christian Gabriel Sanidad
- Adolfo Heruelo Jr.

## What this does

Takes a grayscale image stored as uint8 pixels (0 to 255) and maps every pixel to a single
precision float between 0 and 1, using

```
f / i = 1 / 255   so   f = i * (1/255)
```

C handles the input, the memory allocation and the printing. The conversion itself is done
two ways so they can be compared, once in x86-64 assembly inside `imgCvtGrayInttoFloat` using
scalar SIMD registers (xmm) and scalar SIMD floating point instructions (`cvtsi2ss`, `mulss`,
`movss`, `xorps`), and once in plain C. Both get handed the exact same input buffer, so the
timing comparison is fair and the outputs can be checked against each other.

## Files

| file | what it is |
|---|---|
| `main.c` | C driver, input, allocation, timing, correctness check, output |
| `imgcvt.asm` | the x86-64 asm function, win64 calling convention |
| `build.bat` | windows build |
| `sample_input.txt` | the 3 x 4 example from the specs |

## How to build

Windows, needs nasm and mingw-w64 gcc on PATH:

```
nasm -f win64 imgcvt.asm -o imgcvt.obj
gcc main.c imgcvt.obj -o mp2.exe
```

Or just run `build.bat`.

## How to run

```
mp2.exe                        asks for height, width and pixel values
mp2.exe < sample_input.txt     same thing but fed from a file
mp2.exe bench                  the timing table
mp2.exe demo                   the 3 x 4 sample from the specs
```

Nothing is hardcoded in the default mode, every pixel comes from whoever is typing or from
the input file.

## Correctness

The sample from the specs goes in and both versions come back matching:

```
input (uint8)
 64  89 114  84
140 166 191  84
216 242  38  84

output from x86-64 asm (single float)
0.25 0.35 0.45 0.33
0.55 0.65 0.75 0.33
0.85 0.95 0.15 0.33

output from pure C (single float)
0.25 0.35 0.45 0.33
0.55 0.65 0.75 0.33
0.85 0.95 0.15 0.33

correctness check: asm vs pure C MATCH (0 of 12 pixels differ by more than 1e-06)
```

Every run checks itself. The asm output and the pure C output are compared pixel by pixel with
a tolerance of 1e-6, and if even one pixel is off it prints MISMATCH plus the index and both
values. All sizes match.

## Execution time

The timer wraps the conversion call only, nothing else. 30 runs per version per size, plus one
untimed warmup run so the first timed run is not eating the cold cache cost. Pixels are random
values from 0 to 255 with a fixed seed so the runs repeat, and both versions are handed the
identical image.

Machine: i7-14650HX, 32 GB RAM, Windows 11

Default build, `gcc main.c imgcvt.obj -o mp2.exe`

 size          |    pixels |  asm avg (ms) |    C avg (ms) |  speedup | asm vs C

   10 x 10     |       100 |      0.000050 |      0.000107 |     2.13x | MATCH
  100 x 100    |     10000 |      0.002767 |      0.007697 |     2.78x | MATCH
 1000 x 1000   |   1000000 |      0.270173 |      0.754643 |     2.79x | MATCH

## Analysis

The runtime grows just about linearly with the number of pixels in both versions, which makes
sense because each one does the same small set of steps for every pixel and nothing else, so
the cost is basically per pixel cost times the number of pixels. The 10 x 10 row is the
exception, its time can show up as 0.000000 ms, which does not mean it was free, it means the
conversion finished faster than the timer can measure, since QueryPerformanceCounter only ticks
about every 100 ns. The numbers at that size are noise and should not be read too closely. At
1000 x 1000 the program moves 5 MB of data per call, since the float output takes up four times
the space of the uint8 input, and at that point the limit is how fast memory can feed the CPU
rather than how fast the math runs.

The assembly stays ahead of the pure C version at every size. Part of that is the conversion
itself, the assembly multiplies by a stored 1/255 constant while the C divides by 255, and
divide is considerably slower than multiply on every x86 core. The rest is that the assembly
keeps its running values in registers for the whole loop and only touches memory to read a
pixel and write a float, so there is no stack traffic and a single branch per pixel.