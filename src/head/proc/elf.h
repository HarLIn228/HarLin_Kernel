#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "harlin_API.h"

#define ELFMAG0       0x7F
#define ELFMAG1       'E'
#define ELFMAG2       'L'
#define ELFMAG3       'F'

#define ELFCLASSNONE   0
#define ELFCLASS32     1
#define ELFCLASS64     2

#define ELFDATANONE    0
#define ELFDATA2LSB    1
#define ELFDATA2MSB    2

#define EV_NONE        0
#define EV_CURRENT     1

#define ELFOSABI_NONE        0
#define ELFOSABI_HPUX        1
#define ELFOSABI_LINUX       3

#define ET_NONE        0
#define ET_REL         1
#define ET_EXEC        2
#define ET_DYN         3
#define ET_CORE        4

#define EM_X86_64     62
#define EM_386         3

#define PT_NULL        0
#define PT_LOAD        1
#define PT_DYNAMIC     2
#define PT_INTERP      3
#define PT_NOTE        4
#define PT_SHLIB       5
#define PT_PHDR        6
#define PT_TLS         7
#define PT_GNU_EH_FRAME 0x6474e550
#define PT_GNU_STACK   0x6474e551
#define PT_GNU_RELRO   0x6474e552
#define PT_GNU_PROPERTY 0x6474e553

#define PF_X           0x1
#define PF_W           0x2
#define PF_R           0x4

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB      10
#define SHT_DYNSYM     11
#define SHT_INIT_ARRAY 14
#define SHT_FINI_ARRAY 15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP      17
#define SHT_SYMTAB_SHNDX 18

#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_MERGE       0x10
#define SHF_STRINGS     0x20
#define SHF_INFO_LINK   0x40
#define SHF_LINK_ORDER  0x80
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP       0x200
#define SHF_TLS         0x400
#define SHF_COMPRESSED  0x800

#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2
#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3
#define STT_FILE        4
#define STT_COMMON      5
#define STT_TLS         6

#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_COMMON      0xfff2
#define SHN_ABS         0xfff1

#define DT_NULL         0
#define DT_NEEDED       1
#define DT_PLTRELSZ     2
#define DT_PLTGOT       3
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_RELAENT      9
#define DT_STRSZ       10
#define DT_SYMENT      11
#define DT_INIT        12
#define DT_FINI        13
#define DT_SONAME      14
#define DT_RPATH       15
#define DT_SYMBOLIC    16
#define DT_REL         17
#define DT_RELSZ       18
#define DT_RELENT      19
#define DT_RELRSZ      35
#define DT_RELRENT     36
#define DT_PLTREL      20
#define DT_DEBUG       21
#define DT_TEXTREL     22
#define DT_JMPREL      23
#define DT_BIND_NOW    24
#define DT_INIT_ARRAY  25
#define DT_FINI_ARRAY  26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_RUNPATH     29
#define DT_FLAGS       30
#define DT_PREINIT_ARRAY 32
#define DT_PREINIT_ARRAYSZ 33
#define DT_SYMTAB_SHNDX 34
#define DT_GNU_HASH    0x6ffffef5
#define DT_VERSYM      0x6ffffff0
#define DT_FLAGS_1     0x6ffffffb

#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_PC32       2
#define R_X86_64_GOT32      3
#define R_X86_64_PLT32      4
#define R_X86_64_COPY       5
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8
#define R_X86_64_GOTPCREL   9
#define R_X86_64_32        10
#define R_X86_64_32S       11
#define R_X86_64_16        12
#define R_X86_64_PC16      13
#define R_X86_64_8         14
#define R_X86_64_PC8       15
#define R_X86_64_DTPMOD64  16
#define R_X86_64_DTPOFF64  17
#define R_X86_64_TPOFF64   18
#define R_X86_64_TPOFF32   23

#define ELF32_ST_BIND(i)    ((i) >> 4)
#define ELF32_ST_TYPE(i)    ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))
#define ELF64_ST_BIND(i)    ((i) >> 4)
#define ELF64_ST_TYPE(i)    ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
};

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf64_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct elf64_sym {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t      st_shndx;
    uint64_t      st_value;
    uint64_t      st_size;
};

struct elf64_dyn {
    int64_t  d_tag;
    uint64_t d_val;
};

struct elf64_rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
};

struct elf32_ehdr {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;
    uint32_t      e_phoff;
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
};

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct elf32_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct elf32_sym {
    uint32_t      st_name;
    uint32_t      st_value;
    uint32_t      st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t      st_shndx;
};

struct elf32_rela {
    uint32_t r_offset;
    uint32_t r_info;
    int32_t  r_addend;
};

struct elf32_dyn {
    int32_t  d_tag;
    uint32_t d_val;
};

struct elf32_rel {
    uint32_t r_offset;
    uint32_t r_info;
};

struct elf_info {
    int            is_64;
    int            is_le;
    uint16_t       e_type;
    uint16_t       e_machine;
    uint64_t       e_entry;
    uint16_t       e_phnum;
    uint16_t       e_shnum;
    uint16_t       e_shstrndx;
    const void*    raw;
    uint64_t       raw_size;
};

struct elf_segment {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
    int      loaded;
};

struct elf_section {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
    const char* sh_name_str;
};

struct elf_dyn_state {
    const struct elf64_dyn*  d64;
    const struct elf32_dyn*  d32;
    int                      is_64;
    uint64_t                 strtab;
    uint64_t                 symtab;
    uint64_t                 strsz;
    uint64_t                 syment;
    uint64_t                 pltgot;
    uint64_t                 jmprel;
    uint64_t                 pltrelsz;
    uint64_t                 rela;
    uint64_t                 relasz;
    uint64_t                 relaent;
    uint64_t                 rel;
    uint64_t                 relsz;
    uint64_t                 relent;
    uint64_t                 hash;
    uint64_t                 gnu_hash;
    uint64_t                 init;
    uint64_t                 fini;
    uint64_t                 init_array;
    uint64_t                 fini_array;
    uint64_t                 init_arraysz;
    uint64_t                 fini_arraysz;
    uint64_t                 soname;
    uint64_t                 runpath;
    uint64_t                 flags;
};

int  elf_check_magic(const void* data, uint64_t size);
int  elf_parse_header(const void* data, uint64_t size, struct elf_info* out);
int  elf_load_segment(const struct elf_info* info, const void* data, uint64_t size,
                      int seg_index, struct elf_segment* out);
int  elf_load_all(const struct elf_info* info, const void* data, uint64_t size,
                  struct elf_segment* out_arr, int out_max);
int  elf_get_section(const struct elf_info* info, const void* data, uint64_t size,
                     int idx, struct elf_section* out);
int  elf_find_section(const struct elf_info* info, const void* data, uint64_t size,
                      const char* name, struct elf_section* out);
int  elf_find_symbol(const struct elf_info* info, const void* data, uint64_t size,
                     const char* name, struct elf64_sym* out64, struct elf32_sym* out32);
int  elf_parse_dyn(const struct elf_info* info, const void* dyn, uint64_t size,
                   struct elf_dyn_state* out);
int  elf_apply_relocations(const struct elf_info* info,
                           const struct elf_dyn_state* dyn,
                           int is_rela, const void* relocs, uint64_t relocs_size,
                           uint64_t base_addr, uint64_t load_bias,
                           int (*resolver)(const char* name, uint64_t* out_addr, void* ctx),
                           void* ctx);
int  elf_symbol_name(const struct elf_info* info, const void* data, uint64_t size,
                     const struct elf_section* symtab, const struct elf_section* strtab,
                     int sym_index, char* out_name, int out_max);

struct elf_exec_info {
    uint64_t entry;
    uint64_t phdr_vaddr;
    uint64_t phdr_memsz;
    uint64_t load_bias;
    uint64_t lowest_vaddr;
    uint64_t highest_vaddr;
    int      is_64;
    int      loaded_segments;
    int      total_segments;
};

int  elf_load_exec(const void* data, uint64_t size, struct elf_exec_info* out,
                   int (*alloc_user_page)(unsigned long long vaddr, unsigned long long src_phys, unsigned long long filesz, unsigned long long memsz, void* ctx),
                   void* ctx,
                   const void* data_ptr, uint64_t data_size);
int  elf_load_exec_simple(const void* data, uint64_t size, struct elf_exec_info* out);
int  Harlin_ElfLoadExec(const void* data, uint64_t size, struct elf_exec_info* out);

#define Harlin_ElfCheckMagic          elf_check_magic
#define Harlin_ElfParseHeader         elf_parse_header
#define Harlin_ElfLoadSegment         elf_load_segment
#define Harlin_ElfLoadAll             elf_load_all
#define Harlin_ElfGetSection          elf_get_section
#define Harlin_ElfFindSection         elf_find_section
#define Harlin_ElfFindSymbol          elf_find_symbol
#define Harlin_ElfParseDyn            elf_parse_dyn
#define Harlin_ElfApplyRelocations    elf_apply_relocations
#define Harlin_ElfSymbolName          elf_symbol_name

#endif
