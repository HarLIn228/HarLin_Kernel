#include "elf.h"
#include "kmalloc.h"
#include "harlin_API.h"

static int read_u16(const unsigned char* p, int le)
{
    if (le) return (int)p[0] | ((int)p[1] << 8);
    return ((int)p[0] << 8) | (int)p[1];
}

static uint32_t read_u32(const unsigned char* p, int le)
{
    if (le) return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
           | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t read_u64(const unsigned char* p, int le)
{
    if (le) {
        return (uint64_t)p[0] | ((uint64_t)p[1] << 8)
             | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
             | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
             | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
    }
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] << 8)  | (uint64_t)p[7];
}

static int elf_is_supported_class(unsigned char c)
{
    return c == ELFCLASS32 || c == ELFCLASS64;
}

int elf_check_magic(const void* data, uint64_t size)
{
    const unsigned char* p = (const unsigned char*)data;
    if (!data || size < 16) return -1;
    if (p[0] != ELFMAG0 || p[1] != ELFMAG1
     || p[2] != ELFMAG2 || p[3] != ELFMAG3) return -1;
    if (!elf_is_supported_class(p[4])) return -1;
    if (p[5] != ELFDATA2LSB && p[5] != ELFDATA2MSB) return -1;
    if (p[6] != EV_CURRENT) return -1;
    if (p[7] != ELFOSABI_NONE && p[7] != ELFOSABI_HPUX && p[7] != ELFOSABI_LINUX) return -1;
    return 0;
}

#undef ELFOSABI_NONE
#undef ELFOSABI_HPUX
#undef ELFOSABI_LINUX

#define ELFOSABI_NONE  0
#define ELFOSABI_HPUX  1
#define ELFOSABI_LINUX 3

int elf_parse_header(const void* data, uint64_t size, struct elf_info* out)
{
    const unsigned char* p = (const unsigned char*)data;
    if (elf_check_magic(data, size) != 0) return -1;
    int is_le = (p[5] == ELFDATA2LSB);
    int is_64 = (p[4] == ELFCLASS64);
    if (is_64) {
        if (size < sizeof(struct elf64_ehdr)) return -1;
        const struct elf64_ehdr* eh = (const struct elf64_ehdr*)p;
        out->is_64      = 1;
        out->is_le      = is_le;
        out->e_type     = eh->e_type;
        out->e_machine  = eh->e_machine;
        out->e_entry    = eh->e_entry;
        out->e_phnum    = eh->e_phnum;
        out->e_shnum    = eh->e_shnum;
        out->e_shstrndx = eh->e_shstrndx;
    } else {
        if (size < sizeof(struct elf32_ehdr)) return -1;
        const struct elf32_ehdr* eh = (const struct elf32_ehdr*)p;
        out->is_64      = 0;
        out->is_le      = is_le;
        out->e_type     = eh->e_type;
        out->e_machine  = eh->e_machine;
        out->e_entry    = (uint64_t)eh->e_entry;
        out->e_phnum    = eh->e_phnum;
        out->e_shnum    = eh->e_shnum;
        out->e_shstrndx = eh->e_shstrndx;
    }
    out->raw = data;
    out->raw_size = size;
    return 0;
}

int elf_load_segment(const struct elf_info* info, const void* data, uint64_t size,
                     int seg_index, struct elf_segment* out)
{
    const unsigned char* p = (const unsigned char*)data;
    if (!info || !out) return -1;
    if (seg_index < 0 || seg_index >= info->e_phnum) return -1;
    if (info->is_64) {
        uint64_t phoff = read_u64(p + 32, info->is_le);
        uint16_t phentsize = (uint16_t)read_u16(p + 54, info->is_le);
        uint64_t off = phoff + (uint64_t)seg_index * phentsize;
        if (off + sizeof(struct elf64_phdr) > size) return -1;
        const unsigned char* ph = p + off;
        out->p_type   = read_u32(ph,      info->is_le);
        out->p_flags  = read_u32(ph + 4,  info->is_le);
        out->p_offset = read_u64(ph + 8,  info->is_le);
        out->p_vaddr  = read_u64(ph + 16, info->is_le);
        out->p_paddr  = read_u64(ph + 24, info->is_le);
        out->p_filesz = read_u64(ph + 32, info->is_le);
        out->p_memsz  = read_u64(ph + 40, info->is_le);
        out->p_align  = read_u64(ph + 48, info->is_le);
    } else {
        uint32_t phoff = read_u32(p + 28, info->is_le);
        uint16_t phentsize = (uint16_t)read_u16(p + 42, info->is_le);
        uint64_t off = phoff + (uint64_t)seg_index * phentsize;
        if (off + sizeof(struct elf32_phdr) > size) return -1;
        const unsigned char* ph = p + off;
        out->p_type   = read_u32(ph,      info->is_le);
        out->p_offset = read_u32(ph + 4,  info->is_le);
        out->p_vaddr  = read_u32(ph + 8,  info->is_le);
        out->p_paddr  = read_u32(ph + 12, info->is_le);
        out->p_filesz = read_u32(ph + 16, info->is_le);
        out->p_memsz  = read_u32(ph + 20, info->is_le);
        out->p_flags  = read_u32(ph + 24, info->is_le);
        out->p_align  = read_u32(ph + 28, info->is_le);
    }
    out->loaded = 0;
    return 0;
}

int elf_load_all(const struct elf_info* info, const void* data, uint64_t size,
                 struct elf_segment* out_arr, int out_max)
{
    if (!info || !out_arr) return -1;
    int n = info->e_phnum;
    if (n > out_max) n = out_max;
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (elf_load_segment(info, data, size, i, &out_arr[k]) != 0) continue;
        k++;
    }
    return k;
}

int elf_get_section(const struct elf_info* info, const void* data, uint64_t size,
                    int idx, struct elf_section* out)
{
    const unsigned char* p = (const unsigned char*)data;
    if (!info || !out) return -1;
    if (idx < 0 || idx >= info->e_shnum) return -1;
    if (info->is_64) {
        uint64_t shoff = read_u64(p + 40, info->is_le);
        uint16_t shentsize = (uint16_t)read_u16(p + 58, info->is_le);
        uint64_t off = shoff + (uint64_t)idx * shentsize;
        if (off + sizeof(struct elf64_shdr) > size) return -1;
        const unsigned char* sh = p + off;
        out->sh_name      = read_u32(sh,      info->is_le);
        out->sh_type      = read_u32(sh + 4,  info->is_le);
        out->sh_flags     = read_u64(sh + 8,  info->is_le);
        out->sh_addr      = read_u64(sh + 16, info->is_le);
        out->sh_offset    = read_u64(sh + 24, info->is_le);
        out->sh_size      = read_u64(sh + 32, info->is_le);
        out->sh_link      = read_u32(sh + 40, info->is_le);
        out->sh_info      = read_u32(sh + 44, info->is_le);
        out->sh_addralign = read_u64(sh + 48, info->is_le);
        out->sh_entsize   = read_u64(sh + 56, info->is_le);
    } else {
        uint32_t shoff = read_u32(p + 32, info->is_le);
        uint16_t shentsize = (uint16_t)read_u16(p + 46, info->is_le);
        uint64_t off = shoff + (uint64_t)idx * shentsize;
        if (off + sizeof(struct elf32_shdr) > size) return -1;
        const unsigned char* sh = p + off;
        out->sh_name      = read_u32(sh,      info->is_le);
        out->sh_type      = read_u32(sh + 4,  info->is_le);
        out->sh_flags     = read_u32(sh + 8,  info->is_le);
        out->sh_addr      = read_u32(sh + 12, info->is_le);
        out->sh_offset    = read_u32(sh + 16, info->is_le);
        out->sh_size      = read_u32(sh + 20, info->is_le);
        out->sh_link      = read_u32(sh + 24, info->is_le);
        out->sh_info      = read_u32(sh + 28, info->is_le);
        out->sh_addralign = read_u32(sh + 32, info->is_le);
        out->sh_entsize   = read_u32(sh + 36, info->is_le);
    }
    out->sh_name_str = 0;
    return 0;
}

static int build_sh_name_strtab(const struct elf_info* info, const void* data, uint64_t size,
                                struct elf_section* arr, int count)
{
    if (info->e_shstrndx == 0 || info->e_shstrndx >= info->e_shnum) return 0;
    struct elf_section shstr;
    if (elf_get_section(info, data, size, info->e_shstrndx, &shstr) != 0) return -1;
    const unsigned char* base = (const unsigned char*)data;
    if (shstr.sh_offset + shstr.sh_size > size) return -1;
    const char* strtab = (const char*)(base + shstr.sh_offset);
    for (int i = 0; i < count; i++) {
        uint32_t off = arr[i].sh_name;
        if (off < shstr.sh_size) arr[i].sh_name_str = strtab + off;
        else                     arr[i].sh_name_str = "";
    }
    return 0;
}

int elf_find_section(const struct elf_info* info, const void* data, uint64_t size,
                     const char* name, struct elf_section* out)
{
    if (!info || !name || !out) return -1;
    struct elf_section* tmp = 0;
    int need = info->e_shnum;
    if (need <= 0) return -1;
    tmp = (struct elf_section*)kmalloc((uint64_t)need * sizeof(struct elf_section));
    if (!tmp) return -1;
    for (int i = 0; i < need; i++) {
        if (elf_get_section(info, data, size, i, &tmp[i]) != 0) {
            kfree(tmp);
            return -1;
        }
    }
    build_sh_name_strtab(info, data, size, tmp, need);
    int found = -1;
    for (int i = 0; i < need; i++) {
        if (tmp[i].sh_name_str && Harlin_Compare(tmp[i].sh_name_str, name) == 0) {
            *out = tmp[i];
            found = 0;
            break;
        }
    }
    kfree(tmp);
    return found;
}

int elf_symbol_name(const struct elf_info* info, const void* data, uint64_t size,
                    const struct elf_section* symtab, const struct elf_section* strtab,
                    int sym_index, char* out_name, int out_max)
{
    (void)info;
    (void)size;
    if (!symtab || !strtab || !out_name || out_max <= 0) return -1;
    if (symtab->sh_entsize == 0) return -1;
    uint64_t off = symtab->sh_offset + (uint64_t)sym_index * symtab->sh_entsize;
    uint32_t st_name;
    if (info->is_64) {
        if (off + 4 > size) return -1;
        st_name = *(const uint32_t*)((const unsigned char*)data + off);
    } else {
        if (off + 4 > size) return -1;
        st_name = *(const uint32_t*)((const unsigned char*)data + off);
    }
    if ((uint64_t)st_name >= strtab->sh_size) return -1;
    const char* p = (const char*)((const unsigned char*)data + strtab->sh_offset + st_name);
    int i = 0;
    while (i < out_max - 1 && p[i]) { out_name[i] = p[i]; i++; }
    out_name[i] = 0;
    return 0;
}

int elf_find_symbol(const struct elf_info* info, const void* data, uint64_t size,
                    const char* name, struct elf64_sym* out64, struct elf32_sym* out32)
{
    if (!info || !name) return -1;
    struct elf_section symtab, strtab;
    if (elf_find_section(info, data, size, ".symtab", &symtab) != 0) return -1;
    if (symtab.sh_link == 0 || symtab.sh_link >= info->e_shnum) return -1;
    if (elf_get_section(info, data, size, symtab.sh_link, &strtab) != 0) return -1;
    if (symtab.sh_entsize == 0) return -1;
    uint64_t n = symtab.sh_size / symtab.sh_entsize;
    const unsigned char* base = (const unsigned char*)data;
    for (uint64_t i = 0; i < n; i++) {
        uint64_t off = symtab.sh_offset + i * symtab.sh_entsize;
        uint32_t st_name;
        if (info->is_64) {
            st_name = *(const uint32_t*)(base + off);
        } else {
            st_name = *(const uint32_t*)(base + off);
        }
        if ((uint64_t)st_name >= strtab.sh_size) continue;
        const char* sn = (const char*)(base + strtab.sh_offset + st_name);
        if (Harlin_Compare(sn, name) == 0) {
            if (info->is_64) {
                if (out64) {
                    const struct elf64_sym* p = (const struct elf64_sym*)(base + off);
                    *out64 = *p;
                }
                return 0;
            } else {
                if (out32) {
                    const struct elf32_sym* p = (const struct elf32_sym*)(base + off);
                    *out32 = *p;
                }
                return 0;
            }
        }
    }
    return -1;
}

int elf_parse_dyn(const struct elf_info* info, const void* dyn, uint64_t size,
                  struct elf_dyn_state* out)
{
    if (!info || !dyn || !out) return -1;
    Harlin_Fill((void*)out, 0, sizeof(*out));
    out->d64 = (const struct elf64_dyn*)dyn;
    out->d32 = (const struct elf32_dyn*)dyn;
    out->is_64 = info->is_64;
    int n = (int)(size / (info->is_64 ? 16 : 8));
    for (int i = 0; i < n; i++) {
        int64_t  d_tag;
        uint64_t d_val;
        if (info->is_64) {
            d_tag = (int64_t)out->d64[i].d_tag;
            d_val = out->d64[i].d_val;
        } else {
            d_tag = (int64_t)out->d32[i].d_tag;
            d_val = (uint64_t)out->d32[i].d_val;
        }
        switch (d_tag) {
            case DT_NULL:     return 0;
            case DT_STRTAB:   out->strtab     = d_val; break;
            case DT_SYMTAB:   out->symtab     = d_val; break;
            case DT_STRSZ:    out->strsz      = d_val; break;
            case DT_SYMENT:   out->syment     = d_val; break;
            case DT_PLTGOT:   out->pltgot     = d_val; break;
            case DT_JMPREL:   out->jmprel     = d_val; break;
            case DT_PLTRELSZ: out->pltrelsz   = d_val; break;
            case DT_PLTREL:                         break;
            case DT_RELA:     out->rela       = d_val; break;
            case DT_RELASZ:   out->relasz     = d_val; break;
            case DT_RELAENT:  out->relaent    = d_val; break;
            case DT_REL:      out->rel        = d_val; break;
            case DT_RELSZ:    out->relsz      = d_val; break;
            case DT_RELENT:   out->relent     = d_val; break;
            case DT_HASH:     out->hash       = d_val; break;
            case DT_GNU_HASH: out->gnu_hash   = d_val; break;
            case DT_INIT:     out->init       = d_val; break;
            case DT_FINI:     out->fini       = d_val; break;
            case DT_INIT_ARRAY:   out->init_array    = d_val; break;
            case DT_FINI_ARRAY:   out->fini_array    = d_val; break;
            case DT_INIT_ARRAYSZ: out->init_arraysz  = d_val; break;
            case DT_FINI_ARRAYSZ: out->fini_arraysz  = d_val; break;
            case DT_SONAME:   out->soname     = d_val; break;
            case DT_RUNPATH:  out->runpath    = d_val; break;
            case DT_FLAGS:    out->flags      = d_val; break;
            default: break;
        }
    }
    return 0;
}

static int apply_one_reloc(const struct elf_info* info, const struct elf_dyn_state* dyn,
                           int is_rela, const void* reloc_data,
                           uint64_t base_addr, uint64_t load_bias,
                           int (*resolver)(const char* name, uint64_t* out_addr, void* ctx),
                           void* ctx)
{
    if (!info || !reloc_data) return -1;
    uint64_t r_offset, r_info, r_addend = 0;
    uint32_t r_type, r_sym;
    if (info->is_64) {
        const struct elf64_rela* r = (const struct elf64_rela*)reloc_data;
        r_offset = r->r_offset;
        r_info   = r->r_info;
        r_addend = is_rela ? (uint64_t)r->r_addend : 0;
        r_type   = (uint32_t)r_info;
        r_sym    = (uint32_t)(r_info >> 32);
    } else {
        const struct elf32_rela* r = (const struct elf32_rela*)reloc_data;
        r_offset = r->r_offset;
        r_info   = r->r_info;
        r_addend = is_rela ? (uint64_t)(int64_t)r->r_addend : 0;
        r_type   = (uint32_t)r_info;
        r_sym    = (uint32_t)(r_info >> 8);
    }
    uint64_t target_addr = base_addr + r_offset;
    uint64_t sym_addr = 0;
    if (r_sym != 0 && resolver) {
        if (info->is_64) {
            struct elf64_sym sym;
            const unsigned char* base = (const unsigned char*)info->raw;
            uint64_t off = dyn->symtab + (uint64_t)r_sym * dyn->syment;
            if (off + sizeof(sym) > info->raw_size) return -1;
            Harlin_Copy((void*)&sym, base + off, sizeof(sym));
            if (sym.st_name) {
                char nm[128];
                const unsigned char* s = (const unsigned char*)info->raw + dyn->strtab + sym.st_name;
                int n = 0;
                while (n < 127 && s[n]) { nm[n] = (char)s[n]; n++; }
                nm[n] = 0;
                if (resolver(nm, &sym_addr, ctx) != 0) return -1;
            } else {
                sym_addr = sym.st_value + load_bias;
            }
        } else {
            struct elf32_sym sym;
            const unsigned char* base = (const unsigned char*)info->raw;
            uint64_t off = dyn->symtab + (uint64_t)r_sym * dyn->syment;
            if (off + sizeof(sym) > info->raw_size) return -1;
            Harlin_Copy((void*)&sym, base + off, sizeof(sym));
            if (sym.st_name) {
                char nm[128];
                const unsigned char* s = (const unsigned char*)info->raw + dyn->strtab + sym.st_name;
                int n = 0;
                while (n < 127 && s[n]) { nm[n] = (char)s[n]; n++; }
                nm[n] = 0;
                if (resolver(nm, &sym_addr, ctx) != 0) return -1;
            } else {
                sym_addr = sym.st_value + load_bias;
            }
        }
    } else if (r_sym == 0) {
        sym_addr = 0;
    }
    if (info->is_64) {
        volatile uint64_t* p64 = (volatile uint64_t*)target_addr;
        volatile uint32_t* p32 = (volatile uint32_t*)target_addr;
        volatile uint16_t* p16 = (volatile uint16_t*)target_addr;
        volatile uint8_t*  p8  = (volatile uint8_t*)target_addr;
        switch (r_type) {
            case R_X86_64_NONE:    return 0;
            case R_X86_64_64:      *p64 = sym_addr + r_addend; return 0;
            case R_X86_64_PC32:    *p32 = (uint32_t)(sym_addr + (int64_t)r_addend - (uint64_t)target_addr); return 0;
            case R_X86_64_32:      *p32 = (uint32_t)(sym_addr + r_addend); return 0;
            case R_X86_64_32S:     *p32 = (uint32_t)(int32_t)(sym_addr + (int64_t)r_addend); return 0;
            case R_X86_64_16:      *p16 = (uint16_t)(sym_addr + r_addend); return 0;
            case R_X86_64_8:       *p8  = (uint8_t)(sym_addr + r_addend); return 0;
            case R_X86_64_PC16:    *p16 = (uint16_t)(sym_addr + (int64_t)r_addend - (uint64_t)target_addr); return 0;
            case R_X86_64_PC8:     *p8  = (uint8_t)(sym_addr + (int64_t)r_addend - (uint64_t)target_addr); return 0;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                                  *p64 = sym_addr; return 0;
            case R_X86_64_RELATIVE: *p64 = (uint64_t)(load_bias + (int64_t)r_addend); return 0;
            case R_X86_64_GOTPCREL: *p32 = (uint32_t)(sym_addr + (int64_t)r_addend - (uint64_t)target_addr); return 0;
            case R_X86_64_COPY:    return 0;
            default: return -1;
        }
    } else {
        volatile uint32_t* p32 = (volatile uint32_t*)target_addr;
        volatile uint16_t* p16 = (volatile uint16_t*)target_addr;
        volatile uint8_t*  p8  = (volatile uint8_t*)target_addr;
        switch (r_type) {
            case R_X86_64_NONE:    return 0;
            case R_X86_64_PC32:    *p32 = (uint32_t)(sym_addr + (int64_t)r_addend - (uint64_t)target_addr); return 0;
            case R_X86_64_32:      *p32 = (uint32_t)(sym_addr + r_addend); return 0;
            case R_X86_64_16:      *p16 = (uint16_t)(sym_addr + r_addend); return 0;
            case R_X86_64_8:       *p8  = (uint8_t)(sym_addr + r_addend); return 0;
            case R_X86_64_RELATIVE:*p32 = (uint32_t)(load_bias + (int64_t)r_addend); return 0;
            default: return -1;
        }
    }
}

int elf_apply_relocations(const struct elf_info* info,
                          const struct elf_dyn_state* dyn,
                          int is_rela, const void* relocs, uint64_t relocs_size,
                          uint64_t base_addr, uint64_t load_bias,
                          int (*resolver)(const char* name, uint64_t* out_addr, void* ctx),
                          void* ctx)
{
    if (!info || !dyn || !relocs) return -1;
    if (relocs_size == 0) return 0;
    uint64_t ent = is_rela ? (info->is_64 ? 24 : 12) : (info->is_64 ? 16 : 8);
    if (ent == 0) return -1;
    uint64_t n = relocs_size / ent;
    const unsigned char* p = (const unsigned char*)relocs;
    int applied = 0;
    for (uint64_t i = 0; i < n; i++) {
        if (apply_one_reloc(info, dyn, is_rela, p + i * ent,
                            base_addr, load_bias, resolver, ctx) == 0) {
            applied++;
        }
    }
    return applied;
}

#define ELF_LOAD_BIAS 0x400000ULL

int elf_load_exec(const void* data, uint64_t size, struct elf_exec_info* out,
                  int (*alloc_user_page)(unsigned long long vaddr, unsigned long long src_phys, unsigned long long filesz, unsigned long long memsz, void* ctx),
                  void* ctx,
                  const void* data_ptr, uint64_t data_size)
{
    (void)data_ptr;
    (void)data_size;
    if (!data || !out) return -1;
    if (elf_check_magic(data, size) != 0) return -1;
    struct elf_info info;
    if (elf_parse_header(data, size, &info) != 0) return -1;
    if (info.e_type != ET_EXEC && info.e_type != ET_DYN) return -1;
    if (info.e_machine != EM_X86_64) return -1;

    struct elf_segment segs[16];
    int seg_n = elf_load_all(&info, data, size, segs, 16);
    if (seg_n < 0) seg_n = 0;

    out->is_64 = info.is_64;
    out->load_bias = 0;
    out->lowest_vaddr = (uint64_t)-1;
    out->highest_vaddr = 0;
    out->loaded_segments = 0;
    out->total_segments = 0;
    out->phdr_vaddr = 0;
    out->phdr_memsz = 0;
    out->entry = info.e_entry;

    int i;
    for (i = 0; i < seg_n; i++) {
        if (segs[i].p_type != PT_LOAD) continue;
        out->total_segments++;
        if (segs[i].p_vaddr < out->lowest_vaddr) out->lowest_vaddr = segs[i].p_vaddr;
        uint64_t end = segs[i].p_vaddr + segs[i].p_memsz;
        if (end > out->highest_vaddr) out->highest_vaddr = end;
    }
    if (out->total_segments == 0) return -1;

    if (alloc_user_page) {
        for (i = 0; i < seg_n; i++) {
            if (segs[i].p_type != PT_LOAD) continue;
            uint64_t vaddr = segs[i].p_vaddr;
            uint64_t filesz = segs[i].p_filesz;
            uint64_t memsz = segs[i].p_memsz;
            if (filesz > memsz) filesz = memsz;
            if (alloc_user_page(vaddr, 0, filesz, memsz, ctx) != 0)
                return -1;
        }
        for (i = 0; i < seg_n; i++) {
            if (segs[i].p_type != PT_LOAD) continue;
            if (segs[i].p_filesz == 0) continue;
            uint64_t off_in_file = segs[i].p_offset;
            if (off_in_file > size || segs[i].p_filesz > size - off_in_file) return -1;
            uint64_t left = segs[i].p_filesz;
            uint64_t vaddr = segs[i].p_vaddr;
            const unsigned char* p = (const unsigned char*)data + off_in_file;
            while (left > 0) {
                uint64_t cur_vaddr = vaddr;
                uint64_t page_off = cur_vaddr & 0xFFF;
                uint64_t chunk = 0x1000 - page_off;
                if (chunk > left) chunk = left;
                if (alloc_user_page(cur_vaddr, (uint64_t)p, chunk, chunk, ctx) != 0)
                    return -1;
                p += chunk;
                vaddr += chunk;
                left -= chunk;
            }
        }
        out->loaded_segments = out->total_segments;
    }

    return 0;
}

int elf_load_exec_simple(const void* data, uint64_t size, struct elf_exec_info* out)
{
    return elf_load_exec(data, size, out, 0, 0, 0, 0);
}

int Harlin_ElfLoadExec(const void* data, uint64_t size, struct elf_exec_info* out)
{
    return elf_load_exec_simple(data, size, out);
}
