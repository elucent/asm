#include "asm/arch.h"
#include "util/config.h"
#include "util/io.h"
#include "util/hash.h"

using memory::PAGESIZE;

void LinkedAssembly::load() {
    iword n_code = codesize / PAGESIZE;
    iword n_data = datasize / PAGESIZE;
    iword n_static = statsize / PAGESIZE;
    memory::tag({pages.data(), n_code}, memory::READ | memory::EXEC);
    memory::tag({pages.data() + n_code, n_data}, memory::READ);
    memory::tag({pages.data() + n_code + n_data, n_static}, memory::READ | memory::WRITE);
}

inline iword up_to_nearest_page(iword p) {
    return p + PAGESIZE - 1 & ~(PAGESIZE - 1);
}

void Assembly::linkInto(LinkedAssembly& linked) {
    iword codestart = 0;
    iword datastart = codestart + up_to_nearest_page(code.size());
    iword staticstart = datastart + up_to_nearest_page(data.size());
    iword totalsize = staticstart + up_to_nearest_page(stat.size());
    
    linked.codesize = datastart;
    linked.datasize = staticstart - datastart;
    linked.statsize = totalsize - staticstart;
    linked.pages = memory::map(totalsize / PAGESIZE);
    linked.code = (i8*)linked.pages.data() + codestart;
    linked.data = (i8*)linked.pages.data() + datastart;
    linked.stat = (i8*)linked.pages.data() + staticstart;
    linked.symtab = &symtab;

    code.read(linked.code, code.size());
    data.read(linked.data, data.size());
    stat.read(linked.stat, stat.size());

    for (const Def& def : defs) {
        iptr base;
        switch (def.section) {
            case CODE_SECTION: base = (iptr)linked.code; break;
            case DATA_SECTION: base = (iptr)linked.data; break;
            case STATIC_SECTION: base = (iptr)linked.stat; break;
        }
        linked.defs.put(def.sym, base + def.offset);
    }

    for (const Reloc& ref : relocs) {
        iptr base;
        switch (ref.section) {
            case CODE_SECTION: base = (iptr)linked.code; break;
            case DATA_SECTION: base = (iptr)linked.data; break;
            case STATIC_SECTION: base = (iptr)linked.stat; break;
        }
        iptr reloc = base + ref.offset;
        auto it = linked.defs.find(ref.sym);
        if (it == linked.defs.end())
            panic("Undefined symbol!");
        iptr sym = it->value;

        iptr diff = sym - reloc;
        switch (ref.kind) {
            case Reloc::REL8:
                if (diff < -128 || diff > 127)
                    panic("Difference is too big for 8-bit relative relocation!");
                ((i8*)reloc)[-1] = i8(diff);
                break;
            case Reloc::REL16_LE:
                if (diff < -32768 || diff > 32767)
                    panic("Difference is too big for 16-bit relative relocation!");
                ((i16*)reloc)[-1] = little_endian<i16>(diff);
                break;
            case Reloc::REL32_LE:
                if (diff < -0x80000000l || diff > 0xffffffffl)
                    panic("Difference is too big for 32-bit relative relocation!");
                ((i32*)reloc)[-1] = little_endian<i32>(diff);
                break;
            case Reloc::REL64_LE:
                ((i64*)reloc)[-1] = little_endian<i64>(diff);
                break;
            case Reloc::REL16_BE:
                if (diff < -32768 || diff > 32767)
                    panic("Difference is too big for 16-bit relative relocation!");
                ((i16*)reloc)[-1] = big_endian<i16>(diff);
                break;
            case Reloc::REL32_BE:
                if (diff < -0x80000000l || diff > 0xffffffffl)
                    panic("Difference is too big for 32-bit relative relocation!");
                ((i32*)reloc)[-1] = big_endian<i32>(diff);
                break;
            case Reloc::REL64_BE:
                ((i64*)reloc)[-1] = big_endian<i64>(diff);
                break;
        }
    }

    if (config::printMachineCode) {
        for (i8 i : const_slice<i8>{ linked.code, linked.codesize })
            print(hex((u64)(u8)i, 2));
        println("");
    }
}

struct ELFSymbolInfo {
    u32 index;
    u32 nameOffset;
    u32 offset; // -1 if undefined
    Section section;
    DefType type;
};

void Assembly::writeELFObject(fd file) {
    // Overall ELF relocatable object header

    bytebuf objectHeader;
    objectHeader.write("\x7f" "ELF", 4); // elf magic

    constexpr u8 ELFCLASSNONE = 0, ELFCLASS32 = 1, ELFCLASS64 = 2;
    #ifdef RT_64
    objectHeader.write<u8>(ELFCLASS64); // elf class. we assume 64-bit due to host
    #elif defined(RT_32)
    objectHeader.write<u8>(ELFCLASS32); // elf class. we assume 32-bit due to host
    #else
    #error "Can't generate ELF binaries for non-32-bit, non-64-bit platform."
    #endif

    constexpr u8 ELFDATANONE = 0, ELFDATALSB = 1, ELFDATAMSB = 2;
    objectHeader.write<u8>(ELFDATALSB); // elf data format. we always write little-endian for now

    objectHeader.writeLE<u8>(1); // elf version. always 1 (current version)

    objectHeader.writeLE<u8>(0); // elf os abi. 0 for now, until it becomes important

    objectHeader.writeLE<u8>(0); // elf abi version. 0 for now as well

    objectHeader.write("\0\0\0\0\0\0\0", 7); // elf padding. the identifying header should be 16 bytes
    assert(objectHeader.size() == 16);
    
    constexpr u16 ET_NONE = 0, ET_REL = 1, ET_EXEC = 2, ET_DYN = 3;
    objectHeader.writeLE<u16>(ET_REL); // e_type : u16

    constexpr u16 EM_NONE = 0, EM_X86_64 = 62, EM_AARCH64 = 183;
    #ifdef RT_AMD64
    objectHeader.writeLE<u16>(EM_X86_64); // e_machine : u16
    #elif defined(RT_ARM64)
    objectHeader.writeLE<u16>(EM_AARCH64); // e_machine : u16
    #else
    #error "Can't generate ELF binaries for this machine."
    #endif

    objectHeader.writeLE<u32>(1); // e_version : u32 = 1 (current version)
    objectHeader.writeLE<uptr>(0); // e_entry : uptr = 0x0 (since we're relocatable)
    objectHeader.writeLE<u64>(0); // e_phoff : u64 = 0x0 (again, since we're relocatable)
    objectHeader.writeLE<u64>(64); // e_shoff : u64 = 64 (right after this header)
    objectHeader.writeLE<u32>(0); // e_flags : u32 = 0x0 (no arch-specific flags yet)
    objectHeader.writeLE<u16>(64); // e_ehsize : u16 = 64 (size of ELF header, always the same)
    objectHeader.writeLE<u16>(0); // e_phentsize : u16 = 0 (size of program header table entries, we don't have any)
    objectHeader.writeLE<u16>(0); // e_phnum : u16 = 0 (number of program header table entries, again we don't have any)
    objectHeader.writeLE<u16>(64); // e_shentsize : u16 = 64 (size of section header table entries, fixed for 64-bit binaries)
    objectHeader.writeLE<u16>(10); // e_shnum : u16 = 10 (number of section header table entries, for us that's <null>, .shstrtab, .text, .data, .rodata, .strtab, .symtab, .rel.text, .rel.rodata, .rel.data)
    objectHeader.writeLE<u16>(1); // e_shstrndx : u16 = 1 (index of .shstrtab)
    assert(objectHeader.size() == 64);

    // Section header table. Really we just define it here, and add entries
    // onto it as we generate subsequent sections.

    bytebuf sectionHeaderTable;

    constexpr u32 SHT_NULL = 0, SHT_PROGBITS = 1, SHT_SYMTAB = 2, SHT_STRTAB = 3, SHT_RELA = 4, SHT_REL = 9;
    constexpr u32 SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4, SHF_MERGE = 16, SHF_STRINGS = 32, SHF_INFO_LINK = 64, SHF_TLS = 1024;
    constexpr u32 SHN_UNDEF = 0;

    constexpr u32 offsetOfFirstSection = 64 + 10 * 64; // e_ehsize + e_shnum * e_shentsize

    // Null section

    sectionHeaderTable.writeLE<u32>(0); // sh_name : u32 = 0
    sectionHeaderTable.writeLE<u32>(SHT_NULL); // sh_type : u32 = SHT_NULL
    sectionHeaderTable.writeLE<u64>(0); // sh_flags : u64 = 0
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0
    sectionHeaderTable.writeLE<u64>(0); // sh_offset : u64 (we are the first section)
    sectionHeaderTable.writeLE<u64>(0); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 0
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0
    assert(sectionHeaderTable.size() == 64);

    // Section header string table

    bytebuf sectionHeaderStringTable;
    u32 sectionHeaderStringTableOffset = offsetOfFirstSection;

    sectionHeaderStringTable.write("", 1); // string 0 must be the empty string

    u32 sectionHeaderStringTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".shstrtab", 10); // data for the shstrtab - just some fixed string literals
    u32 textSectionNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".text", 6);
    u32 dataSectionNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".rodata", 8);
    u32 staticSectionNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".data", 6);
    u32 stringTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".strtab", 8);
    u32 symbolTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".symtab", 8);
    u32 textRelocationTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".rela.text", 11);
    u32 dataRelocationTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".rela.rodata", 13);
    u32 staticRelocationTableNameOffset = sectionHeaderStringTable.size();
    sectionHeaderStringTable.write(".rela.data", 11);
    
    while (sectionHeaderStringTable.size() % 64) // pad to multiple of 64 bytes
        sectionHeaderStringTable.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(sectionHeaderStringTableNameOffset); // sh_name : u32 = 1 (.shstrtab)
    sectionHeaderTable.writeLE<u32>(SHT_STRTAB); // sh_type : u32 = SHT_STRTAB (shstrtab is a string table)
    sectionHeaderTable.writeLE<u64>(SHF_STRINGS | SHF_MERGE); // sh_flags : u64 = SHF_STRINGS | SHF_MERGE
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(sectionHeaderStringTableOffset); // sh_offset : u64 (we are the first section)
    sectionHeaderTable.writeLE<u64>(sectionHeaderStringTable.size()); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 0 (we don't care about alignment)
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0 (we don't have any entries with regular size)
    assert(sectionHeaderTable.size() == 128);

    // Text section

    bytebuf textSection;
    u32 textSectionOffset = sectionHeaderStringTableOffset + sectionHeaderStringTable.size();
    assert(textSectionOffset % 64 == 0);
    textSection.write(this->code); // copy in our binary
    while (textSection.size() % 64) // pad to multiple of 64 bytes
        textSection.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(textSectionNameOffset); // sh_name : u32 (.text in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_PROGBITS); // sh_type : u32 = SHT_PROGBITS (code is program data)
    sectionHeaderTable.writeLE<u64>(SHF_EXECINSTR | SHF_ALLOC); // sh_flags : u64 (code is executable and must be allocated at load time)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(textSectionOffset); // sh_offset : u64 (text comes after the section header table)
    sectionHeaderTable.writeLE<u64>(textSection.size()); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(16); // sh_addralign : u64 = 16 (reasonable code alignment requirement)
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0 (we don't have any entries with regular size)
    assert(sectionHeaderTable.size() == 192);

    // Data section

    bytebuf dataSection;
    u32 dataSectionOffset = textSectionOffset + textSection.size();
    assert(dataSectionOffset % 64 == 0);
    dataSection.write(this->data); // copy in our binary
    while (dataSection.size() % 64) // pad to multiple of 64 bytes
        dataSection.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(dataSectionNameOffset); // sh_name : u32 (.rodata in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_PROGBITS); // sh_type : u32 = SHT_PROGBITS (data is program data)
    sectionHeaderTable.writeLE<u64>(SHF_ALLOC); // sh_flags : u64 (must be allocated, but not writable or executable)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(dataSectionOffset); // sh_offset : u64 (text comes after the section header table)
    sectionHeaderTable.writeLE<u64>(dataSection.size()); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(16); // sh_addralign : u64 = 16 (minimal data alignment requirement)
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0 (we don't have any entries with regular size)
    assert(sectionHeaderTable.size() == 256);

    // Static section

    bytebuf staticSection;
    u32 staticSectionOffset = dataSectionOffset + dataSection.size();
    assert(staticSectionOffset % 64 == 0);
    staticSection.write(this->stat); // copy in our binary
    while (staticSection.size() % 64) // pad to multiple of 64 bytes
        staticSection.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(staticSectionNameOffset); // sh_name : u32 (.data in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_PROGBITS); // sh_type : u32 = SHT_PROGBITS (static data is program data)
    sectionHeaderTable.writeLE<u64>(SHF_WRITE | SHF_ALLOC); // sh_flags : u64 (must be allocated and writable, but not executable)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(staticSectionOffset); // sh_offset : u64 (text comes after the section header table)
    sectionHeaderTable.writeLE<u64>(staticSection.size()); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(16); // sh_addralign : u64 = 16 (minimal data alignment requirement)
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0 (we don't have any entries with regular size)
    assert(sectionHeaderTable.size() == 320);

    // String table

    bytebuf stringTable;
    u32 stringTableOffset = staticSectionOffset + staticSection.size();
    assert(stringTableOffset % 64 == 0);
    stringTable.write<u8>(0); // First string is always empty.
    map<Symbol, ELFSymbolInfo> symbols;
    for (Def def : defs) if (!symbols.contains(def.sym))
        symbols.put(def.sym, { 0, 0, (u32)def.offset, def.section, def.type });
    for (Reloc reloc : relocs) if (!symbols.contains(reloc.sym))
        symbols.put(reloc.sym, { 0, 0, 0xffffffffu, CODE_SECTION, DEF_GLOBAL }); // If a symbol is referenced only in relocations, it must not be defined locally, so it must be global.
    u32 cumulativeOffset = 1;
    u32 symbolIndex = 1;
    for (auto& entry : symbols) {
        auto str = symtab[entry.key];
        entry.value.nameOffset = cumulativeOffset;
        entry.value.index = symbolIndex ++;
        stringTable.write(str.data(), str.size());
        cumulativeOffset += str.size();
        if (str.last() != '\0')
            stringTable.write<u8>(0), cumulativeOffset ++;
    }
    while (stringTable.size() % 64) // pad to multiple of 64 bytes
        stringTable.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(stringTableNameOffset); // sh_name : u32 (.strtab in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_STRTAB); // sh_type : u32 = SHT_STRTAB
    sectionHeaderTable.writeLE<u64>(SHF_MERGE | SHF_STRINGS); // sh_flags : u64 (it's a string table, and it's fine to merge it)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(stringTableOffset); // sh_offset : u64 (text comes after the section header table)
    sectionHeaderTable.writeLE<u64>(stringTable.size()); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(SHN_UNDEF); // sh_link : u32 = 0
    sectionHeaderTable.writeLE<u32>(0); // sh_info : u32 = 0
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 0 (we don't care about alignment)
    sectionHeaderTable.writeLE<u64>(0); // sh_entsize : u64 = 0 (we don't have any entries with regular size)
    assert(sectionHeaderTable.size() == 384);

    // Symbol table

    bytebuf symbolTable;
    u32 symbolTableOffset = stringTableOffset + stringTable.size();
    assert(symbolTableOffset % 64 == 0);

    constexpr u8 STB_LOCAL = 0, STB_GLOBAL = 1, STB_WEAK = 2;
    constexpr u8 STT_NOTYPE = 0, STT_OBJECT = 1, STT_FUNC = 2, STT_TLS = 6;
    constexpr u8 STV_DEFAULT = 0, STV_INTERNAL = 1, STV_HIDDEN = 2, STV_EXPORTED = 4, STV_SINGLETON = 5;
    auto symbolInfo = [&](u8 binding, u8 type) -> u8 {
        return binding << 4 | type;
    };

    symbolTable.writeLE<u32>(0); // st_name : u32, for the first symbol this is undefined
    symbolTable.write<u8>(symbolInfo(STB_LOCAL, STT_NOTYPE)); // st_info : u8, doesn't matter since this is a placeholder symbol
    symbolTable.write<u8>(STV_DEFAULT); // st_other : u8, again doesn't matter since this is a placeholder symbol
    symbolTable.writeLE<u16>(0); // st_shndx : u16
    symbolTable.writeLE<uptr>(0); // st_value : uptr = 0
    symbolTable.writeLE<u64>(0); // st_size : u64 = 0

    for (const auto& entry : symbols) {
        u16 shndx;
        switch (entry.value.section) {
            case CODE_SECTION: shndx = 2; break; // .text
            case DATA_SECTION: shndx = 3; break; // .rodata
            case STATIC_SECTION: shndx = 4; break; // .data
            default:
                unreachable("Shouldn't be able to define a symbol in any other section.");
        }
        if (entry.value.offset == 0xffffffffu)
            shndx = SHN_UNDEF;

        symbolTable.writeLE<u32>(entry.value.nameOffset); // st_name : u32 (we generated strings in definition order, so st_name can just be the cumulative offset)
        symbolTable.write<u8>(symbolInfo(entry.value.type == DEF_GLOBAL ? STB_GLOBAL : STB_LOCAL, STT_NOTYPE)); // st_info : u8
        symbolTable.write<u8>(STV_DEFAULT); // st_other : u8, we just use default visibility
        symbolTable.writeLE<u16>(shndx);
        symbolTable.writeLE<uptr>(entry.value.offset == 0xffffffffu ? 0 : entry.value.offset); // st_value : uptr (since it's relative to the start of the section, we can use the same offset)
        symbolTable.writeLE<u64>(0); // st_size : u64 = 0
    }

    while (symbolTable.size() % 64) // pad to multiple of 64 bytes
        symbolTable.write<u8>(0);

    sectionHeaderTable.writeLE<u32>(symbolTableNameOffset); // sh_name : u32 (.symtab in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_SYMTAB); // sh_type : u32 = SHT_SYMTAB
    sectionHeaderTable.writeLE<u64>(SHF_MERGE | SHF_ALLOC); // sh_flags : u64 (it should be fine to merge our symbol table with others)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(symbolTableOffset); // sh_offset : u64 (text comes after the section header table)
    sectionHeaderTable.writeLE<u64>(24 * (symbols.size() + 1)); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(5); // sh_link : u32 = 5 (.strtab)
    sectionHeaderTable.writeLE<u32>(1); // sh_info : u32 = 2 (currently all symbols but the first are global; in the future this should be used to delineate local symbols)
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 8 (minimum alignment for entry type)
    sectionHeaderTable.writeLE<u64>(24); // sh_entsize : u64 = 24 (size of each symbol entry)
    assert(sectionHeaderTable.size() == 448);

    // Relocation tables

    bytebuf textRelocationTable, dataRelocationTable, staticRelocationTable;
    u32 numTextRelocs = 0, numDataRelocs = 0, numStaticRelocs = 0;
    for (Reloc reloc : relocs) {
        bytebuf* buf;
        switch (reloc.section) {
            case CODE_SECTION: buf = &textRelocationTable; numTextRelocs ++; break;
            case DATA_SECTION: buf = &dataRelocationTable; numDataRelocs ++; break;
            case STATIC_SECTION: buf = &staticRelocationTable; numStaticRelocs ++; break;
            default:
                unreachable("Shouldn't have relocations to any other section.");
        }
        uptr offset = reloc.offset;
        u8 type = 0;
        u64 addend = 0;
        #ifdef RT_AMD64
            constexpr u8 R_AMD64_PC16 = 13, R_AMD64_PC32 = 2, R_AMD64_PC8 = 15, R_AMD64_PC64 = 24; 
            switch (reloc.kind) {
                case Reloc::REL8:
                    offset -= 1;
                    type = R_AMD64_PC8;
                    addend = -1;
                    break;
                case Reloc::REL16_LE:
                    offset -= 2;
                    type = R_AMD64_PC16;
                    addend = -2;
                    break;
                case Reloc::REL32_LE:
                    offset -= 4;
                    type = R_AMD64_PC32;
                    addend = -4;
                    break;
                case Reloc::REL64_LE:
                    offset -= 8;
                    type = R_AMD64_PC64;
                    addend = -8;
                    break;
                case Reloc::REL16_BE:
                case Reloc::REL32_BE:
                case Reloc::REL64_BE:
                    unreachable("Shouldn't have big-endian relocations on amd64.");
            }
        #else
            #error "Unsupported architecture for ELF relocations."
        #endif
        buf->writeLE<uptr>(offset); // r_offset : uptr
        buf->writeLE<u64>(u64(symbols[reloc.sym].index) << 32 | type); // r_info : u64
        buf->writeLE<u64>(addend); // r_addend : u64
    }
    while (textRelocationTable.size() % 64) // pad to multiple of 64 bytes
        textRelocationTable.write<u8>(0);
    while (dataRelocationTable.size() % 64)
        dataRelocationTable.write<u8>(0);
    while (staticRelocationTable.size() % 64)
        staticRelocationTable.write<u8>(0);
    u32 textRelocationTableOffset = symbolTableOffset + symbolTable.size();
    u32 dataRelocationTableOffset = textRelocationTableOffset + textRelocationTable.size();
    u32 staticRelocationTableOffset = dataRelocationTableOffset + dataRelocationTable.size();
    assert(textRelocationTableOffset % 64 == 0);
    assert(dataRelocationTableOffset % 64 == 0);
    assert(staticRelocationTableOffset % 64 == 0);

    sectionHeaderTable.writeLE<u32>(textRelocationTableNameOffset); // sh_name : u32 (.rela.text in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_RELA); // sh_type : u32 = SHT_RELA
    sectionHeaderTable.writeLE<u64>(SHF_MERGE | SHF_INFO_LINK); // sh_flags : u64 (it should be fine to merge our relocations with others)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(textRelocationTableOffset); // sh_offset : u64
    sectionHeaderTable.writeLE<u64>(24 * numTextRelocs); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(6); // sh_link : u32 = 6 (.symtab)
    sectionHeaderTable.writeLE<u32>(2); // sh_info : u32 = 2 (relocations apply to .text section)
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 8 (minimum alignment for relocation type)
    sectionHeaderTable.writeLE<u64>(24); // sh_entsize : u64 = 24 (size of each relocation entry)
    assert(sectionHeaderTable.size() == 512);

    sectionHeaderTable.writeLE<u32>(dataRelocationTableNameOffset); // sh_name : u32 (.rela.rodata in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_RELA); // sh_type : u32 = SHT_RELA
    sectionHeaderTable.writeLE<u64>(SHF_MERGE | SHF_INFO_LINK); // sh_flags : u64 (it should be fine to merge our relocations with others)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(dataRelocationTableOffset); // sh_offset : u64
    sectionHeaderTable.writeLE<u64>(24 * numDataRelocs); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(6); // sh_link : u32 = 6 (.symtab)
    sectionHeaderTable.writeLE<u32>(3); // sh_info : u32 = 3 (relocations apply to .rodata section)
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 8 (minimum alignment for relocation type)
    sectionHeaderTable.writeLE<u64>(24); // sh_entsize : u64 = 24 (size of each relocation entry)
    assert(sectionHeaderTable.size() == 576);

    sectionHeaderTable.writeLE<u32>(staticRelocationTableNameOffset); // sh_name : u32 (.rela.data in the section header string table)
    sectionHeaderTable.writeLE<u32>(SHT_RELA); // sh_type : u32 = SHT_RELA
    sectionHeaderTable.writeLE<u64>(SHF_MERGE | SHF_INFO_LINK); // sh_flags : u64 (it should be fine to merge our relocations with others)
    sectionHeaderTable.writeLE<uptr>(0); // sh_addr : uptr = 0 (we're relocatable; someone will fill this in later)
    sectionHeaderTable.writeLE<u64>(staticRelocationTableOffset); // sh_offset : u64
    sectionHeaderTable.writeLE<u64>(24 * numStaticRelocs); // sh_size : u64
    sectionHeaderTable.writeLE<u32>(6); // sh_link : u32 = 6 (.symtab)
    sectionHeaderTable.writeLE<u32>(4); // sh_info : u32 = 2 (relocations apply to .data section)
    sectionHeaderTable.writeLE<u64>(0); // sh_addralign : u64 = 8 (minimum alignment for relocation type)
    sectionHeaderTable.writeLE<u64>(24); // sh_entsize : u64 = 24 (size of each relocation entry)
    assert(sectionHeaderTable.size() == 640);

    write(file, objectHeader);
    write(file, sectionHeaderTable);
    write(file, sectionHeaderStringTable);
    write(file, textSection);
    write(file, dataSection);
    write(file, staticSection);
    write(file, stringTable);
    write(file, symbolTable);
    write(file, textRelocationTable);
    write(file, dataRelocationTable);
    write(file, staticRelocationTable);
}

void LinkedAssembly::writeELFExecutable(fd file) {
    unreachable("TODO: Executable generation");
}
