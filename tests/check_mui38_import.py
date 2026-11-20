#!/usr/bin/env python3
"""MUI 3.8 settings files read by Zune prefs (workbench/prefs/Zune/mui38prefs.c).

The converter is plain C: it is built for the host and run on files made
here the way MUI 3.8's settings program writes them (FORM PREF, PRHD, MUIC
items without padding, MUIW window positions). tests/check_mui38_fuzz.py
uses the driver and helpers defined here.
"""
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'workbench/prefs/Zune'

# conv IN OUT [nofiles]: convert one file (exit 1: not a settings file).
# batch FILE [nofiles]: FILE holds inputs as a 4-byte big-endian length and
# the bytes; each is converted from a buffer of exactly its size into one of
# exactly the documented size, and a converted file must be well formed.
DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mui38prefs.h"

static int no_file(const char *name) { (void)name; return 0; }

static unsigned long get32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16)
        | ((unsigned long)p[2] << 8) | p[3];
}

static unsigned char *slurp(const char *name, long *n)
{
    FILE *f = fopen(name, "rb");
    unsigned char *b;

    if (!f)
        exit(2);
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(*n + 1);
    if (!b || fread(b, 1, *n, f) != (size_t)*n)
        exit(2);
    fclose(f);
    return b;
}

/* a converted file: FORM PREF, PRHD, MUIC whose items fill it exactly */
static int well_formed(const unsigned char *o, long m)
{
    unsigned long n, p;

    if (m < MUI38_ITEMS_OFFSET || (m & 1) || get32(o + 4) != (unsigned long)m - 8
        || memcmp(o + 8, "PREFPRHD", 8) || memcmp(o + 26, "MUIC", 4))
        return 0;
    n = get32(o + 30);
    if (MUI38_ITEMS_OFFSET + n > (unsigned long)m || m - (MUI38_ITEMS_OFFSET + n) > 1)
        return 0;
    for (p = MUI38_ITEMS_OFFSET; p + 8 <= MUI38_ITEMS_OFFSET + n; )
        p += 8 + get32(o + p + 4);
    return p == MUI38_ITEMS_OFFSET + n;
}

int main(int argc, char **argv)
{
    mui38_have_file have = argc > 3 ? no_file : 0;
    long n, m;

    if (argc >= 4 && !strcmp(argv[1], "conv"))
    {
        unsigned char *in = slurp(argv[2], &n), *out = malloc(n + 64);
        FILE *f;

        m = mui38_convert(in, n, out, n + 64, argc > 4 ? no_file : 0);
        if (m < 0)
            return 1;
        if (!well_formed(out, m) || !(f = fopen(argv[3], "wb"))
            || fwrite(out, 1, m, f) != (size_t)m)
            return 2;
        fclose(f);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "batch"))
    {
        unsigned char *all = slurp(argv[2], &n);
        long p = 0, cases = 0, ok = 0;

        while (p + 4 <= n)
        {
            long len = (long)get32(all + p);
            unsigned char *in, *out;

            if (len > n - p - 4)
                return 2;
            in = malloc(len ? len : 1);
            out = malloc(len + 64);
            memcpy(in, all + p + 4, len);
            m = mui38_convert(in, len, out, len + 64, have);
            if (m > len + 64 || (m >= 0 && !well_formed(out, m)))
            {
                printf("case %ld: bad output (%ld bytes)\n", cases, m);
                return 3;
            }
            ok += m >= 0;
            cases++;
            free(in);
            free(out);
            p += 4 + len;
        }
        printf("%ld %ld\n", cases, ok);
        return 0;
    }
    return 2;
}
'''


def host_cc():
    cc = os.environ.get('HOSTCC') or shutil.which('cc') or shutil.which('gcc')
    return cc if cc and shutil.which(cc) else None


def build(tmp, flags):
    """The converter and DRIVER as a host program; None if it cannot be built."""
    cc = host_cc()
    if not cc:
        return None
    driver = Path(tmp) / 'mui38conv.c'
    driver.write_text(DRIVER)
    exe = Path(tmp) / 'mui38conv'
    r = subprocess.run([cc, *flags, '-Wall', '-Werror', '-I', str(SRC), '-o', str(exe),
                        str(driver), str(SRC / 'mui38prefs.c')],
                       capture_output=True, text=True)
    if r.returncode:
        print(r.stderr.strip()[:2000])
        return None
    return exe


def chunk(cid, data):
    return cid + struct.pack('>I', len(data)) + data + (b'\0' if len(data) & 1 else b'')


def item(iid, value):
    if isinstance(value, int):
        value = struct.pack('>I', value)
    elif isinstance(value, str):
        value = value.encode('latin1') + b'\0'
    return struct.pack('>II', iid, len(value)) + value


def form(*chunks):
    body = b'PREF' + b''.join(chunks)
    return b'FORM' + struct.pack('>I', len(body)) + body


def window(wid, *words):
    """A MUIW entry: id, length 40, 20 WORDs (the rest zero)"""
    words = list(words) + [0] * (20 - len(words))
    return wid + struct.pack('>I', 40) + struct.pack('>20h', *words)


def sample():
    """A MUI 3.8 file with every kind of item the converter translates"""
    muiw = (window(b'MAIN', 70, 30, 500, 196, 70, 30, 500, 196, *([0] * 10), 1, 0)
            + window(b'SUBW', 10, 20, 300, 100)
            + window(b'NONE', 5, 5, 0, 0))            # no width: left out
    return form(chunk(b'PRHD', bytes([0x13, 0, 0, 0, 0, 0])),
                chunk(b'MUIC', b''.join([
                    item(0x24, 'TESTSCR'),             # public screen: same id
                    item(0x29, 'ctrl alt z'),          # iconify hotkey
                    item(0x7c, 0), item(0x7d, 1),      # pop to front, show menu
                    item(0x14, 0), item(0x15, 1),      # show icon, on startup
                    item(0x7e, 0),                     # ARexx
                    item(0x81, 22), item(0x82, 4),     # bubble delays, tenths of s
                    item(0x0e, 2), item(0x1c, 1),      # frame thickness, refresh
                    item(0x13, 1), item(0x76, 0), item(0x77, 1), item(0x78, 1),
                    item(0x2b, '602211'), item(0x33, '916644'),
                    item(0x34, '112211'), item(0x90, 'a02200'),
                    item(0x2d, 'A12211'), item(0x32, ':10000'), item(0x18, '700000'),
                    item(0x46, '3:XEN/CheckMark.mf0'),
                    item(0x48, '3:XEN/Plain/11pt/Cycle.mbr'),
                    item(0x4c, '3:XEN/Scrollbars/xenbar.image'),
                    item(0x42, '3:kmel/KMEL_ARROWUP.IMAGE'),
                    item(0x4d, '4:Goonie/Drawer.mbr'),
                    item(0x37, '2:m2'), item(0x54, '0:139'), item(0x3b, '5:Patterns/x.iff'),
                    item(0x8b, 'alt'),
                    item(0x80020001, b'2:p4' + b'\0' * 60),  # another class's item
                ])),
                chunk(b'MUIW', muiw))


def run(exe, src, tmp, *args):
    (Path(tmp) / 'in.prefs').write_bytes(src)
    r = subprocess.run([str(exe), 'conv', str(Path(tmp) / 'in.prefs'),
                        str(Path(tmp) / 'out.prefs'), *args])
    return r.returncode, (Path(tmp) / 'out.prefs').read_bytes() if r.returncode == 0 else None


def items(data):
    """The MUIC items of a converted file, as the library's Dataspace reads them"""
    assert data[:4] == b'FORM' and data[8:12] == b'PREF'
    assert struct.unpack('>I', data[4:8])[0] == len(data) - 8
    out, p = {}, 12
    while p + 8 <= len(data):
        cid, n = data[p:p + 4], struct.unpack('>I', data[p + 4:p + 8])[0]
        if cid == b'MUIC':
            q, end = p + 8, p + 8 + n
            while q + 8 <= end:
                iid, ln = struct.unpack('>II', data[q:q + 8])
                assert iid not in out, hex(iid)
                out[iid] = data[q + 8:q + 8 + ln]
                q += 8 + ln
            assert q == end
        p += 8 + n + (n & 1)
    return out


def main():
    tmp = tempfile.mkdtemp(prefix='mui38imp')
    try:
        exe = build(tmp, ['-O1'])
        if not exe:
            print('check_mui38_import: no host C compiler, skipped')
            return
        code, out = run(exe, sample(), tmp)
        assert code == 0
        got = items(out)
        u = lambda i: struct.unpack('>I', got[i])[0]
        s = lambda i: got[i].rstrip(b'\0').decode('latin1')
        assert s(0x24) == 'TESTSCR'
        assert s(0x701) == 'ctrl alt z'
        assert (u(0x700), u(0x703), u(0x702), u(0x704), u(0x705)) == (0, 1, 0, 1, 0)
        assert (u(0x706), u(0x707)) == (22, 4)
        assert u(0x404) == 1
        assert u(0x0e) == 2                          # no Zune setting: kept as it is
        assert u(0x50a) == 4 | 1 | 8                 # iconify, prefs (MUI), popup
        for gone in (0x1c, 0x13, 0x14, 0x15, 0x29, 0x76, 0x77, 0x78,
                     0x7c, 0x7d, 0x7e, 0x81, 0x82):
            assert gone not in got, hex(gone)
        # frame types 5-10 renumbered, 'A' and ':' are 10
        assert (s(0x2b), s(0x33), s(0x34), s(0x90)) == ('902211', '716644', '112211', '502200')
        assert (s(0x2d), s(0x32), s(0x18)) == ('512211', '510000', 'a00000')
        # brushes are "4:", image classes stay "3:"
        assert s(0x46) == '4:XEN/CheckMark.mf0'
        assert s(0x48) == '4:XEN/Plain/11pt/Cycle.mbr'
        assert s(0x4c) == '3:XEN/Scrollbars/xenbar.image'
        assert s(0x42) == '3:kmel/KMEL_ARROWUP.IMAGE'
        assert s(0x4d) == '4:Goonie/Drawer.mbr'
        assert (s(0x37), s(0x54), s(0x3b), s(0x8b)) == ('2:m2', '0:139', '5:Patterns/x.iff', 'alt')
        assert got[0x80020001] == b'2:p4' + b'\0' * 60
        # window positions: MAIN with its second box, SUBW with its box twice
        size, = struct.unpack('>I', got[0x509][:4])
        assert size == 4 + 2 * 20 == len(got[0x509])
        assert got[0x509][4:24] == b'MAIN' + struct.pack('>8h', 70, 30, 500, 196, 70, 30, 500, 196)
        assert got[0x509][24:44] == b'SUBW' + struct.pack('>8h', 10, 20, 300, 100, 10, 20, 300, 100)

        # pictures and classes that cannot be found are left out
        code, out = run(exe, sample(), tmp, 'nofiles')
        got = items(out)
        for gone in (0x46, 0x48, 0x4c, 0x42, 0x4d, 0x3b):
            assert gone not in got, hex(gone)
        assert 0x37 in got and 0x54 in got

        # no more windows than the library's table holds
        many = b''.join(window(struct.pack('>I', 0x1000 + i), i, i, 100, 50)
                        for i in range(320))
        code, out = run(exe, form(chunk(b'PRHD', bytes(6)), chunk(b'MUIC', b''),
                                  chunk(b'MUIW', many)), tmp)
        wp = items(out)[0x509]
        assert struct.unpack('>I', wp[:4])[0] == len(wp) == 4 + 299 * 20

        # not a settings file
        assert run(exe, b'FORM\0\0\0\4ILBM', tmp)[0] == 1
        assert run(exe, b'FORM', tmp)[0] == 1
        # a file MUI 3.8 wrote with nothing changed
        code, out = run(exe, form(chunk(b'PRHD', bytes(6)), chunk(b'MUIC', b'')), tmp)
        assert code == 0 and items(out) == {}
        # odd chunks are padded; a MUIC after an odd one is still read
        code, out = run(exe, form(chunk(b'JUNK', b'abc'), chunk(b'MUIC', item(0x07, 8))), tmp)
        assert items(out) == {0x07: struct.pack('>I', 8)}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('check_mui38_import: MUI 3.8 settings conversion ok')


if __name__ == '__main__':
    main()
