#include "asm/arch.h"
#include "util/config.h"

using namespace memory;

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
