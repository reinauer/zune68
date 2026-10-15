// census_callers.mjs <perfcensus count transcript> [nm -n of the unstripped library] [top]: symbolize the "caller <fn> <ret> <count>" lines per phase
import {readFileSync} from 'node:fs';
const lines = readFileSync(process.argv[2], 'utf8').split('\n');
const nmFile = process.argv[3] || ''; const top = Number(process.argv[4] || 12);
const syms = [];
if (nmFile) for (const l of readFileSync(nmFile, 'utf8').split('\n')) { const m = l.match(/^([0-9a-f]+) [tT] (\S+)/); if (m) syms.push([parseInt(m[1], 16), m[2]]); }
syms.sort((a, b) => a[0] - b[0]);
let base = null;
const m0 = lines.find(l => l.startsWith('muimaster base='));
if (m0) base = parseInt(m0.match(/layout-vector=([0-9a-f]+)/)[1], 16) - 0x32c;
const size = syms.length ? syms[syms.length - 1][0] + 0x1000 : 0;
function sym(addr) {
  if (base !== null && syms.length && addr >= base && addr < base + size) {
    const off = addr - base; let lo = 0, hi = syms.length - 1, best = null;
    while (lo <= hi) { const mid = (lo + hi) >> 1; if (syms[mid][0] <= off) { best = syms[mid]; lo = mid + 1; } else hi = mid - 1; }
    return best ? best[1] + '+' + (off - best[0]).toString(16) : 'zune?';
  }
  return (addr >= 0xf80000 && addr < 0x1000000) ? 'rom:' + addr.toString(16) : 'ram:' + addr.toString(16);
}
let phase = null; const per = new Map();
for (const l of lines) {
  if (l.startsWith('phase ')) { phase = l.split(' ')[1]; per.set(phase, new Map()); }
  else if (l.startsWith('caller ') && phase) {
    const [, fn, ret, count] = l.split(' ');
    const m = per.get(phase); if (!m.has(fn)) m.set(fn, []);
    m.get(fn).push([parseInt(ret, 16), Number(count)]);
  }
}
for (const [ph, fns] of per) {
  console.log('== ' + ph);
  for (const [fn, list] of [...fns].sort((a, b) => b[1].reduce((s, x) => s + x[1], 0) - a[1].reduce((s, x) => s + x[1], 0))) {
    const total = list.reduce((s, x) => s + x[1], 0);
    const rows = list.sort((a, b) => b[1] - a[1]).slice(0, top).map(([a, c]) => c + ' ' + sym(a)).join('; ');
    console.log(`  ${fn} (${total} calls, ${list.length} sites): ${rows}`);
  }
}
