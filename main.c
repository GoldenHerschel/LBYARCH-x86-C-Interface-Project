/*
 * main.c
 * C side of the project
 * this file grabs the input, allocates the buffers, calls the asm function,
 * checks the result against a plain C version and prints everything out
 * the actual conversion math lives in imgcvt.asm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define RUNS 30          /* spec says at least 30 runs for the average */
#define TOL  1e-6f       /* how far off a pixel can be before we call it wrong */

/* this one is written in x86-64 assembly, see imgcvt.asm */
extern void imgCvtGrayInttoFloat(int height, int width, unsigned char *in, float *out);

/* plain C version, only here so we have something to compare the asm output against */
static void refCvtGrayInttoFloat(int height, int width, unsigned char *in, float *out)
{
    int i;
    int total = height * width;
    for (i = 0; i < total; i++)
        out[i] = (float)in[i] / 255.0f;
}

/* returns wall clock seconds, uses whatever high res timer the OS gives us */
static double now_sec(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

/* walks both arrays and counts how many pixels do not match */
static int check(float *got, float *want, int total, int *first_bad)
{
    int i, bad = 0;
    *first_bad = -1;
    for (i = 0; i < total; i++) {
        if (fabsf(got[i] - want[i]) > TOL) {
            if (bad == 0) *first_bad = i;
            bad++;
        }
    }
    return bad;
}

static void print_float_img(float *img, int height, int width)
{
    int r, c;
    for (r = 0; r < height; r++) {
        for (c = 0; c < width; c++)
            printf("%.2f ", img[r * width + c]);
        printf("\n");
    }
}

/* reads height, width then height*width integers from stdin */
static void run_input_mode(void)
{
    int height, width, total, i, bad, first_bad, val;
    unsigned char *in;
    float *out, *ref;

    printf("enter height and width: ");
    if (scanf("%d %d", &height, &width) != 2 || height <= 0 || width <= 0) {
        printf("bad size input\n");
        return;
    }

    total = height * width;
    in  = (unsigned char *)malloc(total * sizeof(unsigned char));
    out = (float *)malloc(total * sizeof(float));
    ref = (float *)malloc(total * sizeof(float));
    if (!in || !out || !ref) {
        printf("malloc failed\n");
        free(in); free(out); free(ref);
        return;
    }

    printf("enter %d pixel values 0 to 255:\n", total);
    for (i = 0; i < total; i++) {
        if (scanf("%d", &val) != 1) {
            printf("ran out of pixel values\n");
            free(in); free(out); free(ref);
            return;
        }
        if (val < 0)   val = 0;      /* clamp so nothing weird gets in */
        if (val > 255) val = 255;
        in[i] = (unsigned char)val;
    }

    imgCvtGrayInttoFloat(height, width, in, out);
    refCvtGrayInttoFloat(height, width, in, ref);

    printf("\ninput (uint8)\n");
    for (i = 0; i < total; i++) {
        printf("%3d ", in[i]);
        if ((i + 1) % width == 0) printf("\n");
    }

    printf("\noutput (single float, from asm)\n");
    print_float_img(out, height, width);

    bad = check(out, ref, total, &first_bad);
    printf("\ncorrectness check: %s (%d of %d pixels off by more than %g)\n",
           bad == 0 ? "PASSED" : "FAILED", bad, total, TOL);
    if (bad)
        printf("first mismatch at index %d, asm gave %.9f, C gave %.9f\n",
               first_bad, out[first_bad], ref[first_bad]);

    free(in); free(out); free(ref);
}

/* fills the image with random pixels, runs the asm function 30 times and averages */
static void bench_one(int height, int width)
{
    int total = height * width;
    int i, r, bad, first_bad;
    double t0, sum = 0.0, best = 1e30, worst = 0.0, dt;
    unsigned char *in;
    float *out, *ref;

    in  = (unsigned char *)malloc((size_t)total * sizeof(unsigned char));
    out = (float *)malloc((size_t)total * sizeof(float));
    ref = (float *)malloc((size_t)total * sizeof(float));
    if (!in || !out || !ref) {
        printf("could not allocate %d pixels, skipping this size\n", total);
        free(in); free(out); free(ref);
        return;
    }

    for (i = 0; i < total; i++)
        in[i] = (unsigned char)(rand() % 256);

    /* one warmup call so the cache is not cold on the first timed run */
    imgCvtGrayInttoFloat(height, width, in, out);

    for (r = 0; r < RUNS; r++) {
        t0 = now_sec();
        imgCvtGrayInttoFloat(height, width, in, out);
        dt = now_sec() - t0;
        sum += dt;
        if (dt < best)  best = dt;
        if (dt > worst) worst = dt;
    }

    refCvtGrayInttoFloat(height, width, in, ref);
    bad = check(out, ref, total, &first_bad);

    printf("%5d x %-5d | %10d | %12.6f | %12.6f | %12.6f | %s\n",
           height, width, total,
           (sum / RUNS) * 1000.0, best * 1000.0, worst * 1000.0,
           bad == 0 ? "PASSED" : "FAILED");

    free(in); free(out); free(ref);
}

static void run_bench_mode(void)
{
    printf("timing the asm function only, %d runs per size\n\n", RUNS);
    printf(" size         |     pixels |     avg (ms) |     min (ms) |     max (ms) | correct\n");
    printf("--------------+------------+--------------+--------------+--------------+--------\n");
    bench_one(10, 10);
    bench_one(100, 100);
    bench_one(1000, 1000);
    printf("\n");
}

/* runs the 3x4 sample straight out of the specs so you can eyeball it */
static void run_demo_mode(void)
{
    unsigned char sample[12] = { 64,  89, 114, 84,
                                140, 166, 191, 84,
                                216, 242,  38, 84 };
    float out[12], ref[12];
    int bad, first_bad;

    printf("sample image from the specs, 3 x 4\n\n");
    printf("input (uint8)\n");
    {
        int r, c;
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 4; c++)
                printf("%3d ", sample[r * 4 + c]);
            printf("\n");
        }
    }

    imgCvtGrayInttoFloat(3, 4, sample, out);
    refCvtGrayInttoFloat(3, 4, sample, ref);

    printf("\noutput (single float, from asm)\n");
    print_float_img(out, 3, 4);

    bad = check(out, ref, 12, &first_bad);
    printf("\ncorrectness check: %s (%d of 12 pixels off)\n",
           bad == 0 ? "PASSED" : "FAILED", bad);
}

int main(int argc, char **argv)
{
    srand(12345);   /* fixed seed so the runs are repeatable */

    printf("grayscale uint8 to single float converter\n");
    printf("conversion is done in x86-64 asm with scalar SIMD\n\n");

    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        run_bench_mode();
    } else if (argc > 1 && strcmp(argv[1], "input") == 0) {
        run_input_mode();
    } else {
        run_demo_mode();
        printf("\n");
        run_bench_mode();
        printf("run with the arg 'input' if you want to type your own image in\n");
    }

    return 0;
}
