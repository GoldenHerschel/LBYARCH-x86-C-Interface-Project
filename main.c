/*
 * main.c
 * Christian Sanidad, Adolfo Heruelo
 *
 * C side of the project
 * this file grabs the input, allocates the buffers, runs the conversion two ways,
 * the pure C version and the x86-64 asm version, then checks that the two agree
 * and prints everything out
 * the asm conversion lives in imgcvt.asm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#define RUNS 30          /* spec says at least 30 runs for the average */
#define TOL  1e-6f       /* how far off a pixel can be before we call it wrong */

/* this one is written in x86-64 assembly, see imgcvt.asm */
extern void imgCvtGrayInttoFloat(int height, int width, unsigned char *in, float *out);

/* pure C version of the exact same conversion, we time it and compare against it */
static void imgCvtGrayInttoFloatC(int height, int width, unsigned char *in, float *out)
{
    int i;
    int total = height * width;
    for (i = 0; i < total; i++)
        out[i] = (float)in[i] / 255.0f;
}

/* returns wall clock seconds using the windows high resolution counter */
static double now_sec(void)
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}

/* walks both arrays and counts how many pixels do not match */
static int check(float *a, float *b, int total, int *first_bad)
{
    int i, bad = 0;
    *first_bad = -1;
    for (i = 0; i < total; i++) {
        if (fabsf(a[i] - b[i]) > TOL) {
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

static void print_uint8_img(unsigned char *img, int height, int width)
{
    int r, c;
    for (r = 0; r < height; r++) {
        for (c = 0; c < width; c++)
            printf("%3d ", img[r * width + c]);
        printf("\n");
    }
}

/* runs both versions on the same image and prints both outputs plus the verdict */
static void convert_and_report(int height, int width, unsigned char *in)
{
    int total = height * width;
    int bad, first_bad;
    float *out_asm, *out_c;

    out_asm = (float *)malloc(total * sizeof(float));
    out_c   = (float *)malloc(total * sizeof(float));
    if (!out_asm || !out_c) {
        printf("malloc failed\n");
        free(out_asm); free(out_c);
        return;
    }

    /* same input pointer goes to both so the comparison is fair */
    imgCvtGrayInttoFloat(height, width, in, out_asm);
    imgCvtGrayInttoFloatC(height, width, in, out_c);

    printf("\ninput (uint8)\n");
    print_uint8_img(in, height, width);

    printf("\noutput from x86-64 asm (single float)\n");
    print_float_img(out_asm, height, width);

    printf("\noutput from pure C (single float)\n");
    print_float_img(out_c, height, width);

    bad = check(out_asm, out_c, total, &first_bad);
    printf("\ncorrectness check: asm vs pure C %s (%d of %d pixels differ by more than %g)\n",
           bad == 0 ? "MATCH" : "MISMATCH", bad, total, TOL);
    if (bad)
        printf("first mismatch at index %d, asm gave %.9f, C gave %.9f\n",
               first_bad, out_asm[first_bad], out_c[first_bad]);

    free(out_asm); free(out_c);
}

/* reads height, width then height*width integers from whoever is typing */
static void run_input_mode(void)
{
    int height, width, total, i, val;
    unsigned char *in;

    printf("enter height and width: ");
    if (scanf("%d %d", &height, &width) != 2 || height <= 0 || width <= 0) {
        printf("bad size input\n");
        return;
    }

    total = height * width;
    in = (unsigned char *)malloc(total * sizeof(unsigned char));
    if (!in) {
        printf("malloc failed\n");
        return;
    }

    printf("enter %d pixel values 0 to 255:\n", total);
    for (i = 0; i < total; i++) {
        if (scanf("%d", &val) != 1) {
            printf("ran out of pixel values\n");
            free(in);
            return;
        }
        if (val < 0)   val = 0;      /* clamp so nothing weird gets in */
        if (val > 255) val = 255;
        in[i] = (unsigned char)val;
    }

    convert_and_report(height, width, in);
    free(in);
}

/* random image, times both versions RUNS times each and prints one row of the table */
static void bench_one(int height, int width)
{
    int total = height * width;
    int i, r, bad, first_bad;
    double t0, sum_asm = 0.0, sum_c = 0.0;
    unsigned char *in;
    float *out_asm, *out_c;

    in      = (unsigned char *)malloc((size_t)total * sizeof(unsigned char));
    out_asm = (float *)malloc((size_t)total * sizeof(float));
    out_c   = (float *)malloc((size_t)total * sizeof(float));
    if (!in || !out_asm || !out_c) {
        printf("could not allocate %d pixels, skipping this size\n", total);
        free(in); free(out_asm); free(out_c);
        return;
    }

    /* one random image, both versions get the exact same pixels so it is a fair race */
    for (i = 0; i < total; i++)
        in[i] = (unsigned char)(rand() % 256);

    /* untimed warmup so the first timed run is not paying for a cold cache */
    imgCvtGrayInttoFloat(height, width, in, out_asm);
    imgCvtGrayInttoFloatC(height, width, in, out_c);

    for (r = 0; r < RUNS; r++) {
        t0 = now_sec();
        imgCvtGrayInttoFloat(height, width, in, out_asm);
        sum_asm += now_sec() - t0;

        t0 = now_sec();
        imgCvtGrayInttoFloatC(height, width, in, out_c);
        sum_c += now_sec() - t0;
    }

    bad = check(out_asm, out_c, total, &first_bad);

    printf(" %4d x %-6d | %9d | %13.6f | %13.6f | %8.2fx | %s\n",
           height, width, total,
           (sum_asm / RUNS) * 1000.0,
           (sum_c   / RUNS) * 1000.0,
           (sum_asm > 0.0) ? (sum_c / sum_asm) : 0.0,
           bad == 0 ? "MATCH" : "MISMATCH");

    free(in); free(out_asm); free(out_c);
}

static void run_bench_mode(void)
{
    printf("timing the conversion only, %d runs per version per size\n", RUNS);
    printf("random pixel values, both versions get the identical input\n\n");
    printf(" size          |    pixels |  asm avg (ms) |    C avg (ms) |  speedup | asm vs C\n");
    printf("---------------+-----------+---------------+---------------+----------+---------\n");
    bench_one(10, 10);
    bench_one(100, 100);
    bench_one(1000, 1000);
    printf("\n");
}

/* the 3 x 4 sample straight out of the specs, only for eyeballing, not the default mode */
static void run_demo_mode(void)
{
    unsigned char sample[12] = { 64,  89, 114, 84,
                                140, 166, 191, 84,
                                216, 242,  38, 84 };

    printf("canned sample image from the specs, 3 x 4\n");
    convert_and_report(3, 4, sample);
}

int main(int argc, char **argv)
{
    srand(12345);   /* fixed seed so the benchmark runs repeat */

    printf("grayscale uint8 to single float converter\n");
    printf("conversion done two ways, x86-64 asm with scalar SIMD and pure C\n");

    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        printf("\n");
        run_bench_mode();
    } else if (argc > 1 && strcmp(argv[1], "demo") == 0) {
        printf("\n");
        run_demo_mode();
    } else {
        printf("modes: mp2.exe bench for the timing table, mp2.exe demo for the spec sample\n\n");
        run_input_mode();
    }

    return 0;
}