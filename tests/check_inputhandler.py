#!/usr/bin/env python3
"""Check timer callback messages through linked Application input dispatch.

Only Exec ports and timer delivery are modeled. The actual input loop must
rearm each request and supply its originating handler in the callback.
"""
import sys
from pathlib import Path
from check_application_services import ApplicationHarness, INPUT
from check_class_lifetime import EXEC

TIMER_PORT, METHOD = 0x72c00, 0x81726801


def check(path):
    h = ApplicationHarness(path)
    h.field('timer', TIMER_PORT)
    h.mem.w8(TIMER_PORT + 15, 23)
    h.queues[TIMER_PORT] = []
    signals = h.words(1 << 23)
    expected, events = [], []

    def send():
        req = h.cpu.r_reg(9)
        events.append(('send', req, h.mem.r32(req + 32),
                       h.mem.r32(req + 36)))

    def callback():
        args = tuple(h.argument(i) for i in range(4))
        assert args == expected.pop(0), args
        events.append(('call', args[3]))
        h.cpu.w_reg(0, 0)

    h.trap(EXEC - 462, send)
    h.trap(h.symbols['_DoMethod'], callback)
    requests = []
    for i, millis in enumerate((40, 1500)):
        handler = h.blob(bytes(24))
        destination = 0x72d00 + 32 * i
        h.mem.w32(handler + 8, destination)
        h.mem.w16(handler + 12, millis)
        h.mem.w32(handler + 16, 1)  # MUIIHNF_TIMER
        h.mem.w32(handler + 20, METHOD + i)
        request = h.blob(bytes(44))  # timerequest followed by handler pointer
        h.mem.w32(request + 40, handler)
        requests.append((request, handler, destination, millis))

    # Multiple ready timers and a later repeat retain the correct node.
    for batch in (requests, requests[:1]):
        h.mem.w32(signals, 1 << 23)
        for request, handler, destination, millis in batch:
            h.queues[TIMER_PORT].append(request)
            expected.append((destination, h.mem.r32(handler + 20), 0, handler))
        before = len(events)
        h.dispatch(INPUT, signals)
        assert not expected
        wanted = []
        for request, handler, destination, millis in batch:
            wanted += [('send', request, millis // 1000,
                        millis % 1000 * 1000), ('call', handler)]
        assert events[before:] == wanted
    before = len(events)
    h.dispatch(INPUT, signals)
    assert len(events) == before  # Empty timer port does not invoke callbacks.
    h.finish()
    print(f'{path}: timer callback arguments and rearming checks passed')


if __name__ == '__main__':
    for name in sys.argv[1:]:
        check(Path(name))
