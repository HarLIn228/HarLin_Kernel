#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <process.h>

#define CHC_HEADER_SIZE 512
#define USER_MAX_SIZE   0x100000

#define ESC "\x1b"
#define GREEN ESC "[32m"
#define RED   ESC "[31m"
#define RESET ESC "[0m"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;

#pragma pack(push, 1)
typedef struct {
    u8  magic[9];
    u8  reserved[7];
    u16 version;
    u16 flags;
    u32 entry_offset;
    u64 code_offset;
    u64 code_size;
    u64 data_offset;
    u64 data_size;
    u64 bss_size;
    u64 reloc_offset;
    u64 reloc_count;
    u64 stack_size;
} chc_header;

#pragma pack(push, 1)
typedef struct {
    u16 machine;
    u16 num_sections;
    u32 timestamp;
    u32 symtab_off;
    u32 num_symbols;
    u16 optional_header_size;
    u16 characteristics;
} coff_header;

typedef struct {
    u8  name[8];
    u32 virtual_size;
    u32 virtual_address;
    u32 raw_size;
    u32 raw_off;
    u32 reloc_off;
    u32 line_off;
    u16 num_relocs;
    u16 num_lines;
    u32 characteristics;
} coff_section;

typedef struct {
    u32 virtual_address;
    u32 symbol_index;
    u16 type;
} coff_reloc;
#pragma pack(pop)

#define IMAGE_FILE_MACHINE_AMD64 0x8664

#define IMAGE_REL_AMD64_ADDR64   0x0001
#define IMAGE_REL_AMD64_ADDR32   0x0002
#define IMAGE_REL_AMD64_ADDR32NB 0x0003
#define IMAGE_REL_AMD64_REL32    0x0004
#define IMAGE_REL_AMD64_REL32_1  0x0005
#define IMAGE_REL_AMD64_REL32_2  0x0006
#define IMAGE_REL_AMD64_REL32_3  0x0007

static char *input_file = NULL;
static char *output_file = NULL;
static char *entry_sym = "_start";
static u64 stack_size = 0;

static void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s[Error]%s ", RED, RESET);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    void *data;
    size_t size;
    if (!f) fatal("cannot open %s", path);
    fseek(f, 0, SEEK_END);
    size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc(size);
    if (!data) fatal("out of memory");
    if (fread(data, 1, size, f) != size) fatal("cannot read %s", path);
    fclose(f);
    *out_size = size;
    return data;
}

static void write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) fatal("cannot create %s", path);
    if (fwrite(data, 1, size, f) != size) fatal("cannot write %s", path);
    fclose(f);
}

static int is_code_sec(const char *name)
{
    return strcmp(name, ".text") == 0 || strcmp(name, ".code") == 0;
}

static int is_data_sec(const char *name)
{
    return strcmp(name, ".data") == 0 || strcmp(name, ".rdata") == 0;
}

static int is_bss_sec(const char *name)
{
    return strcmp(name, ".bss") == 0;
}

static const char *sec_name(const coff_section *s)
{
    static char buf[16];
    if (s->name[0] == '/') {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }
    memcpy(buf, s->name, 8);
    buf[8] = 0;
    return buf;
}

static void print_help(void)
{
    printf("hcc: HarLin C Compiler\n");
    printf("usage: hcc [options] <source.c>\n");
    printf("options:\n");
    printf("  -o <file>     output CHC file\n");
    printf("  -e <symbol>   entry point symbol (default: _start)\n");
    printf("  -h            show this help\n");
}

static void parse_args(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) fatal("missing argument for -o");
            output_file = argv[i];
        } else if (strcmp(argv[i], "-e") == 0) {
            if (++i >= argc) fatal("missing argument for -e");
            entry_sym = argv[i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        } else if (argv[i][0] == '-') {
            fatal("unknown option %s", argv[i]);
        } else {
            if (input_file) fatal("multiple input files");
            input_file = argv[i];
        }
    }
    if (!input_file) fatal("no input file");
    if (!output_file) {
        output_file = (char *)malloc(strlen(input_file) + 5);
        if (!output_file) fatal("out of memory");
        strcpy(output_file, input_file);
        char *dot = strrchr(output_file, '.');
        if (dot) *dot = 0;
        strcat(output_file, ".chc");
    }
}

static void compile_to_obj(const char *src, const char *obj)
{
    char cmd[1024];
    const char *cc = getenv("CC");
    if (!cc) cc = "x86_64-w64-mingw32-gcc";
    snprintf(cmd, sizeof(cmd),
             "%s -c -O2 -Wall -Wextra -ffreestanding -fno-exceptions "
             "-fno-stack-protector -fno-stack-check -nostdlib -nodefaultlibs "
             "-I hcc/include -o %s %s",
             cc, obj, src);
    printf("%s[1/4]%s Compiling %s to object\n", GREEN, RESET, src);
    if (system(cmd) != 0) fatal("compilation failed");
}

int main(int argc, char **argv)
{
    char obj_path[256];
    void *data;
    size_t size;
    const coff_header *ch;
    const coff_section *secs;
    const coff_reloc *rels;
    u8 *code_buf = NULL;
    u8 *data_buf = NULL;
    u64 code_size = 0;
    u64 data_size = 0;
    u64 bss_size = 0;
    u64 *reloc_buf = NULL;
    u64 reloc_count = 0;
    u64 code_file_off, data_file_off, reloc_file_off;
    u64 entry_offset = 0;
    int found_text = 0;
    int found_entry = 0;
    int i, j;
    chc_header hdr;
    u8 *out;
    u64 out_size;

    parse_args(argc, argv);

    snprintf(obj_path, sizeof(obj_path), "hcc_%s.o", input_file);
    for (i = 0; obj_path[i]; i++) {
        if (obj_path[i] == '/' || obj_path[i] == '\\' || obj_path[i] == '.')
            obj_path[i] = '_';
    }

    compile_to_obj(input_file, obj_path);

    printf("%s[2/4]%s Parsing object file %s\n", GREEN, RESET, obj_path);
    data = read_file(obj_path, &size);

    if (size < sizeof(coff_header))
        fatal("file too small");
    ch = (const coff_header *)data;
    if (ch->machine != IMAGE_FILE_MACHINE_AMD64)
        fatal("unsupported machine (need AMD64)");

    secs = (const coff_section *)((u8 *)data + sizeof(coff_header) + ch->optional_header_size);

    printf("%s[3/4]%s Extracting sections\n", GREEN, RESET);
    for (i = 0; i < ch->num_sections; i++) {
        const char *name = sec_name(&secs[i]);
        if (is_code_sec(name)) {
            if (found_text) fatal("multiple .text sections");
            found_text = 1;
            code_size = secs[i].raw_size;
            if (code_size == 0) fatal(".text section is empty");
            if (code_size > USER_MAX_SIZE) fatal(".text section too large");
            code_buf = (u8 *)malloc(code_size);
            if (!code_buf) fatal("out of memory");
            memcpy(code_buf, (u8 *)data + secs[i].raw_off, code_size);
        } else if (is_data_sec(name)) {
            u64 old = data_size;
            u64 add = secs[i].raw_size;
            data_size += add;
            if (data_size > USER_MAX_SIZE) fatal(".data section too large");
            data_buf = (u8 *)realloc(data_buf, data_size);
            if (!data_buf) fatal("out of memory");
            memcpy(data_buf + old, (u8 *)data + secs[i].raw_off, add);
        } else if (is_bss_sec(name)) {
            bss_size += secs[i].virtual_size;
            if (bss_size > USER_MAX_SIZE) fatal(".bss section too large");
        }
    }

    if (!found_text) fatal("no .text section found");

    rels = NULL;
    for (i = 0; i < ch->num_sections; i++) {
        if (!is_code_sec(sec_name(&secs[i]))) continue;
        if (secs[i].num_relocs == 0) continue;
        rels = (const coff_reloc *)((u8 *)data + secs[i].reloc_off);
        for (j = 0; j < secs[i].num_relocs; j++) {
            u16 type = rels[j].type;
            u64 off = rels[j].virtual_address;
            if (off + 8 > code_size) fatal("relocation out of bounds");
            if (type == IMAGE_REL_AMD64_ADDR64) {
                u64 val = *(u64 *)(code_buf + off);
                if (val >= 0x100000000ULL) fatal("unsupported 64-bit relocation value");
            } else if (type == IMAGE_REL_AMD64_ADDR32 ||
                       type == IMAGE_REL_AMD64_ADDR32NB) {
                s32 val = *(s32 *)(code_buf + off);
                if ((u64)(val < 0 ? -val : val) >= USER_MAX_SIZE)
                    fatal("unsupported 32-bit relocation value");
            } else {
                continue;
            }
            reloc_buf = (u64 *)realloc(reloc_buf, (reloc_count + 1) * sizeof(u64));
            if (!reloc_buf) fatal("out of memory");
            reloc_buf[reloc_count++] = off;
        }
    }

    entry_offset = 0;
    found_entry = 0;
    if (ch->num_symbols > 0 && ch->symtab_off > 0) {
        const u8 *symtab = (const u8 *)data + ch->symtab_off;
        const char *strtab = (const char *)symtab + ch->num_symbols * 18;
        for (i = 0; i < (int)ch->num_symbols; i++) {
            const u8 *sym = symtab + i * 18;
            char name[256];
            if (sym[0] == 0 && sym[1] == 0 && sym[2] == 0 && sym[3] == 0) {
                u32 off = *(const u32 *)(sym + 4);
                strncpy(name, strtab + off, sizeof(name) - 1);
                name[sizeof(name) - 1] = 0;
            } else {
                memcpy(name, sym, 8);
                name[8] = 0;
            }
            if (strcmp(name, entry_sym) == 0) {
                u32 value = *(const u32 *)(sym + 8);
                entry_offset = value;
                found_entry = 1;
                break;
            }
        }
    }
    if (!found_entry) fatal("entry symbol %s not found", entry_sym);

    code_file_off = CHC_HEADER_SIZE;
    data_file_off = code_file_off + ((code_size + 7) & ~7ULL);
    reloc_file_off = data_file_off + ((data_size + 7) & ~7ULL);
    out_size = reloc_file_off + reloc_count * sizeof(u64);
    out = (u8 *)calloc(1, out_size);
    if (!out) fatal("out of memory");

    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "HARLINCHC", 9);
    hdr.version = 1;
    hdr.flags = 0;
    hdr.entry_offset = (u32)entry_offset;
    hdr.code_offset = code_file_off;
    hdr.code_size = code_size;
    hdr.data_offset = data_file_off;
    hdr.data_size = data_size;
    hdr.bss_size = bss_size;
    hdr.reloc_offset = reloc_file_off;
    hdr.reloc_count = reloc_count;
    hdr.stack_size = stack_size;

    memcpy(out, &hdr, sizeof(hdr));
    memcpy(out + code_file_off, code_buf, code_size);
    if (data_size > 0) memcpy(out + data_file_off, data_buf, data_size);
    if (reloc_count > 0) memcpy(out + reloc_file_off, reloc_buf, reloc_count * sizeof(u64));

    printf("%s[4/4]%s Generating %s\n", GREEN, RESET, output_file);
    write_file(output_file, out, out_size);

    remove(obj_path);
    free(data);
    free(code_buf);
    free(data_buf);
    free(reloc_buf);
    free(out);

    printf("%s[Success]%s Created %s\n", GREEN, RESET, output_file);
    return 0;
}
