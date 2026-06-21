#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

static void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bin2h: error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void usage(void)
{
    printf("bin2h: binary to C header converter\n");
    printf("usage: bin2h <input.bin> <output.h> [symbol_name]\n");
    exit(0);
}

int main(int argc, char **argv)
{
    FILE *in, *out;
    const char *symbol;
    size_t size, i;
    uint8_t byte;

    if (argc < 3) usage();

    in = fopen(argv[1], "rb");
    if (!in) fatal("cannot open %s", argv[1]);

    out = fopen(argv[2], "wb");
    if (!out) fatal("cannot create %s", argv[2]);

    symbol = (argc >= 4) ? argv[3] : "binary_data";

    fseek(in, 0, SEEK_END);
    size = (size_t)ftell(in);
    fseek(in, 0, SEEK_SET);

    fprintf(out, "#ifndef %s_H\n", symbol);
    fprintf(out, "#define %s_H\n\n", symbol);
    fprintf(out, "#include \"harlin_API.h\"\n\n");
    fprintf(out, "static const u8 %s[] = {\n    ", symbol);

    for (i = 0; i < size; i++) {
        if (fread(&byte, 1, 1, in) != 1)
            fatal("cannot read %s", argv[1]);
        fprintf(out, "0x%02X, ", byte);
        if ((i + 1) % 12 == 0) fprintf(out, "\n    ");
    }

    fprintf(out, "\n};\n\n");
    fprintf(out, "static const u64 %s_size = %zu;\n\n", symbol, size);
    fprintf(out, "#endif\n");

    fclose(in);
    fclose(out);

    printf("bin2h: created %s (%zu bytes)\n", argv[2], size);
    return 0;
}
