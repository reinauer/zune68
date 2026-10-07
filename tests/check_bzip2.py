#!/usr/bin/env python3
"""Exercise bounded bzip2 decoding with an independent encoder and guards."""
import bz2
import ctypes as C
import os
import random
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OK, STREAM_END, PARAM_ERROR, DATA_ERROR, MAGIC_ERROR, MEM_ERROR = 0, 4, -2, -4, -5, -3
GUARD = b'\xa5' * 16
Alloc = C.CFUNCTYPE(C.c_void_p, C.c_void_p, C.c_int, C.c_int)
Free = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p)


class Stream(C.Structure):
    _fields_ = [('next_in', C.c_void_p), ('avail_in', C.c_uint),
                ('total_in_lo32', C.c_uint), ('total_in_hi32', C.c_uint),
                ('next_out', C.c_void_p), ('avail_out', C.c_uint),
                ('total_out_lo32', C.c_uint), ('total_out_hi32', C.c_uint),
                ('state', C.c_void_p), ('bzalloc', C.c_void_p),
                ('bzfree', C.c_void_p), ('opaque', C.c_void_p)]


class Allocator:
    """Guard every decoder allocation and optionally fail one allocation."""
    def __init__(self, fail=0):
        self.fail = fail
        self.requests = []
        self.live = {}
        self.freed = []
        self.errors = []
        self.peak = 0
        self.alloc_callback = Alloc(self.alloc)
        self.free_callback = Free(self.free)

    def alloc(self, opaque, items, size):
        length = items * size
        self.requests.append(length)
        if len(self.requests) == self.fail:
            return None
        assert length > 0
        block = C.create_string_buffer(length + 32)
        C.memset(block, 0xa5, len(block))
        address = C.addressof(block) + 16
        self.live[address] = (block, length)
        self.peak = max(self.peak, sum(size for _, size in self.live.values()))
        return address

    def free(self, opaque, address):
        block = self.live.pop(address, None)
        if block is None:
            self.errors.append(f'free of unknown address {address}')
        else:
            self.freed.append(block)

    def verify(self):
        assert not self.errors, self.errors
        for block, size in [*self.live.values(), *self.freed]:
            assert block.raw[:16] == GUARD
            assert block.raw[size + 16:size + 32] == GUARD
        assert not self.live, f'{len(self.live)} leaked allocations'


def decode(lib, encoded, capacity, small, limit=None, fail=0, chunk=None):
    """Return status, output and allocations; output capacity remains enforced.

    The bounded initializer limits scratch storage, not total decoded bytes.
    The caller must still provide and check its own output buffer capacity.
    """
    allocator = Allocator(fail)
    stream = Stream()
    stream.bzalloc = C.cast(allocator.alloc_callback, C.c_void_p).value
    stream.bzfree = C.cast(allocator.free_callback, C.c_void_p).value
    if limit is None:
        result = lib.BZ2_bzDecompressInit(C.byref(stream), 0, small)
    else:
        result = lib.BZ2_bzDecompressInitBounded(C.byref(stream), 0, small, limit)
    source = C.create_string_buffer(encoded)
    target = C.create_string_buffer(capacity + 32)
    C.memset(target, 0xa5, len(target))
    output_address = C.addressof(target) + 16
    stream.next_in = C.addressof(source)
    stream.next_out = output_address
    supplied = 0
    stream.avail_out = capacity if chunk is None else min(capacity, chunk)
    try:
        while result == OK:
            if not stream.avail_in and supplied < len(encoded):
                length = len(encoded) - supplied
                if chunk is not None:
                    length = min(length, chunk)
                stream.avail_in = length
                supplied += length
            remaining = capacity - stream.total_out_lo32
            if not stream.avail_out and remaining:
                stream.avail_out = remaining if chunk is None else min(remaining, chunk)
            before = (stream.total_in_lo32, stream.total_out_lo32)
            result = lib.BZ2_bzDecompress(C.byref(stream))
            # A streaming decoder reports OK when it needs more input/output.
            if before == (stream.total_in_lo32, stream.total_out_lo32):
                break
        output = target.raw[16:16 + stream.total_out_lo32]
        assert target.raw[:16] == GUARD
        assert target.raw[16 + capacity:32 + capacity] == GUARD
        assert stream.total_out_lo32 <= capacity
        consumed = stream.total_in_lo32
    finally:
        if stream.state:
            assert lib.BZ2_bzDecompressEnd(C.byref(stream)) == OK
        allocator.verify()
    return result, output, consumed, allocator


def check():
    with tempfile.TemporaryDirectory(prefix='zune68-bzip2-') as directory:
        root = Path(directory)
        stub = root / 'assert.c'
        stub.write_text('#include <stdlib.h>\nvoid bz_internal_error(int n) { abort(); }\n')
        library = root / 'decoder.so'
        subprocess.run([os.environ.get('HOSTCC', 'cc'), '-shared', '-fPIC', '-O2',
                        '-DBZ_NO_STDIO', *map(str, (ROOT / 'vendor/bzip2').glob('*.c')),
                        str(stub), '-o', str(library)], check=True)
        lib = C.CDLL(str(library))
        lib.BZ2_bzDecompressInit.argtypes = [C.POINTER(Stream), C.c_int, C.c_int]
        lib.BZ2_bzDecompressInitBounded.argtypes = [C.POINTER(Stream), C.c_int,
                                                 C.c_int, C.c_uint]
        for name in ['BZ2_bzDecompress', 'BZ2_bzDecompressEnd']:
            getattr(lib, name).argtypes = [C.POINTER(Stream)]
        for name in ['BZ2_bzDecompressInit', 'BZ2_bzDecompressInitBounded',
                     'BZ2_bzDecompress', 'BZ2_bzDecompressEnd']:
            getattr(lib, name).restype = C.c_int
        rng = random.Random(68)
        # Four-byte runs exercise the worst-case expansion in bzip2's first RLE.
        runs = b''.join(bytes([value]) * 4 for value in range(256))
        samples = [b'', b'x', b'aaaabbbbcccc', b'x' * 500000,
                   runs * 1024, bytes(range(256)) * 4096, rng.randbytes(180000)]
        cases = 0
        for level in [1, 5, 9]:
            for sample in samples:
                encoded = bz2.compress(sample, compresslevel=level)
                for small in [0, 1]:
                    for limit in [None, max(1, len(sample))]:
                        status, output, consumed, _ = decode(
                            lib, encoded, len(sample), small, limit)
                        assert (status, output, consumed) == (STREAM_END, sample, len(encoded))
                        cases += 1
        # State must survive fragmented input and output, including one byte at a time.
        sample = runs * 2 + rng.randbytes(1024)
        encoded = bz2.compress(sample)
        for small in [0, 1]:
            for limit in [None, len(sample)]:
                for chunk in [1, 7, 127]:
                    status, output, consumed, _ = decode(
                        lib, encoded, len(sample), small, limit, chunk=chunk)
                    assert (status, output, consumed) == (STREAM_END, sample, len(encoded))
                    cases += 1
                # Every prefix is incomplete, never a successful stream.
                for cut in range(len(encoded)):
                    status, _, _, _ = decode(lib, encoded[:cut], len(sample), small, limit)
                    assert status != STREAM_END, cut
                    assert status in (OK, DATA_ERROR, MAGIC_ERROR), (cut, status)
                    cases += 1
                # Deliberate damage spans headers, transform data and stream CRC.
                for position in range(0, len(encoded), max(1, len(encoded) // 80)):
                    damaged = bytearray(encoded)
                    damaged[position] ^= 0x40
                    try:
                        reference = bz2.decompress(damaged)
                    except OSError:
                        reference = None
                    status, output, _, _ = decode(lib, bytes(damaged), len(sample), small, limit)
                    if reference is None:
                        assert status in (OK, DATA_ERROR, MAGIC_ERROR), (position, status)
                    else:
                        assert status == STREAM_END and output == reference
                    cases += 1
                # Each allocation failure must unwind all successful allocations.
                _, _, _, normal = decode(lib, encoded, len(sample), small, limit)
                for failure in range(1, len(normal.requests) + 1):
                    status, _, _, _ = decode(lib, encoded, len(sample), small, limit, fail=failure)
                    assert status == MEM_ERROR, (small, limit, failure, status)
                    cases += 1
        # A BZh9 header must not force a 900K-entry allocation for a tiny icon.
        sample = rng.randbytes(24 * 19 * 4)
        encoded = bz2.compress(sample, compresslevel=9)
        for small in [0, 1]:
            for limit in [1, len(sample), 719999, 720000, 0xffffffff, None]:
                status, output, _, allocations = decode(lib, encoded, len(sample), small, limit)
                capacity = 900000 if limit is None or limit >= 720000 else min(900000, limit + limit // 4 + 4)
                scratch = ([capacity * 2, (capacity + 1) // 2]
                           if small else [capacity * 4])
                assert allocations.requests[1:] == scratch, (limit, allocations.requests)
                if limit == 1:
                    assert status == DATA_ERROR
                else:
                    assert status == STREAM_END and output == sample
                cases += 1
            # Undersized output never writes beyond caller's buffer, for either API.
            for limit in [None, len(sample)]:
                for capacity in [0, 1, len(sample) - 1]:
                    status, output, _, _ = decode(lib, encoded, capacity, small, limit)
                    assert status == OK and output == sample[:capacity]
                    cases += 1
            status, _, _, allocations = decode(lib, encoded, len(sample), small, limit=0)
            assert status == PARAM_ERROR and not allocations.requests
            cases += 1
    print(f'bzip2: {cases} cases passed; bounded/default APIs, fragmented and corrupt '
          'streams, allocation failures and guarded buffers')


if __name__ == '__main__':
    check()
