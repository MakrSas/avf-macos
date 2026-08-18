import struct
import sys

FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9

def main(path):
    data = open(path, 'rb').read()
    magic, totalsize, off_dt_struct, off_dt_strings, off_mem_rsvmap, version, \
        last_comp_version, boot_cpuid_phys, size_dt_strings, size_dt_struct = \
        struct.unpack('>10I', data[0:40])
    print(f"magic=0x{magic:x} totalsize={totalsize} version={version}")

    p = off_dt_struct
    end = off_dt_struct + size_dt_struct
    depth = 0

    def getstr(off):
        s = data[off_dt_strings+off:]
        return s[:s.index(b'\0')].decode()

    while p < end:
        tok = struct.unpack('>I', data[p:p+4])[0]
        p += 4
        if tok == FDT_BEGIN_NODE:
            nameend = data.index(b'\0', p)
            name = data[p:nameend].decode()
            p = nameend + 1
            p = (p + 3) & ~3
            print("  " * depth + f"NODE {name if name else '/'} {{")
            depth += 1
        elif tok == FDT_END_NODE:
            depth -= 1
            print("  " * depth + "}")
        elif tok == FDT_PROP:
            length, nameoff = struct.unpack('>II', data[p:p+8])
            p += 8
            val = data[p:p+length]
            pname = getstr(nameoff)
            p += length
            p = (p + 3) & ~3
            # try to print as hex u32 array if length is multiple of 4 and property looks numeric
            printable = all(32 <= b < 127 for b in val[:-1]) and val.endswith(b'\0') and len(val) > 0
            if pname in ('reg', 'ranges', 'interrupts', 'clock-frequency', 'phandle',
                          'interrupt-parent', '#address-cells', '#size-cells',
                          'msi-parent', 'interrupt-map', 'interrupt-map-mask'):
                nums = []
                for i in range(0, len(val) - 3, 4):
                    nums.append(struct.unpack('>I', val[i:i+4])[0])
                hexs = " ".join(f"0x{n:x}" for n in nums)
                print("  " * depth + f"{pname} = <{hexs}>")
            elif printable:
                s = val.split(b'\0')[0].decode(errors='replace')
                print("  " * depth + f"{pname} = \"{s}\"")
            else:
                print("  " * depth + f"{pname} = (len={length}) {val[:32].hex()}")
        elif tok == FDT_NOP:
            pass
        elif tok == FDT_END:
            break
        else:
            print(f"unknown token {tok} at {p}")
            break

if __name__ == '__main__':
    main(sys.argv[1])
