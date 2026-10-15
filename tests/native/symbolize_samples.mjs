// symbolize_samples.mjs <samples.json> [nm -n of the unstripped library] [top]: attribute samples to Zune68 functions (code base from muimaster LVO -126 and the nm offset of _Gate_MUI_Layout, 0x32c) and ROM/library vectors
import {readFileSync} from 'node:fs';
const data = JSON.parse(readFileSync(process.argv[2], 'utf8'));
const nmFile = process.argv[3] || '';
const top = Number(process.argv[4] || 40);
const vec = []; // [addr, label]
let zuneBase = null, zuneSize = 0;
for (const lib of data.libs) {
  const short = lib.name.split('.')[0];
  for (const [lvo, target] of lib.vectors) vec.push([target, short + ':-' + lvo]);
  if (lib.name === 'muimaster.library') {
    const t = lib.vectors.find(v => v[0] === 126);
    if (t) zuneBase = t[1] - 0x32c; // Gate_MUI_Layout is at 0x32c in nm-head.txt
  }
}
vec.sort((a, b) => a[0] - b[0]);
const syms = [];
if (nmFile) for (const l of readFileSync(nmFile, 'utf8').split('\n')) {
  const m = l.match(/^([0-9a-f]+) [tT] (\S+)/); if (m) syms.push([parseInt(m[1], 16), m[2]]);
}
syms.sort((a, b) => a[0] - b[0]);
if (syms.length) zuneSize = syms[syms.length - 1][0] + 0x1000;
function lookup(table, pc, maxdist) {
  let lo = 0, hi = table.length - 1, best = null;
  while (lo <= hi) { const mid = (lo + hi) >> 1; if (table[mid][0] <= pc) { best = table[mid]; lo = mid + 1; } else hi = mid - 1; }
  return best && pc - best[0] < maxdist ? best[1] : null;
}
const isZune = pc => zuneBase !== null && pc >= zuneBase && pc < zuneBase + zuneSize;
const zuneSym = pc => lookup(syms, pc - zuneBase, 0x4000) || 'zune?';
const romSym = pc => lookup(vec, pc, 0x3000) || ('rom ' + (pc >> 12).toString(16) + 'xxx');
const counts = new Map(), callers = new Map(), byCaller = new Map(), tasks = new Map();
const add = (m, k) => m.set(k, (m.get(k) || 0) + 1);
let tot = 0, zune = 0, rom = 0, other = 0, irq = 0;
const inTimer = a => a >= 0xfc61a4 && a < 0xfc6fd4; // timer.device ROM code (A1200 47.115)
for (const [pc, ret, tname, tstate] of data.samples) {
  if (ret.some(inTimer) || inTimer(pc)) { irq++; continue; } // interrupt-coincident sample
  tot++;
  add(tasks, (tname || '?') + (tstate !== undefined ? ' state=' + tstate : ''));
  let key;
  if (isZune(pc)) { zune++; key = 'zune ' + zuneSym(pc); }
  else if ((pc >= 0xf80000 && pc < 0x1000000) || lookup(vec, pc, 0x3000)) { rom++; key = 'rom ' + romSym(pc); }
  else { other++; key = 'other ' + (pc >> 12).toString(16) + 'xxx'; }
  add(counts, key);
  // nearest Zune return address on the stack = the toolkit function responsible
  const caller = ret.find(isZune);
  const ck = caller !== undefined ? zuneSym(caller) : (isZune(pc) ? zuneSym(pc) : '(none)');
  add(byCaller, (isZune(pc) ? '' : 'via ') + ck + (isZune(pc) ? '' : ' -> ' + key.replace(/^rom /, '')));
}
console.log(`interrupt-coincident samples set aside: ${irq} of ${irq + tot}`);
console.log(`samples ${tot}: zune ${(100 * zune / tot).toFixed(1)}% rom ${(100 * rom / tot).toFixed(1)}% other ${(100 * other / tot).toFixed(1)}%  zuneBase ${zuneBase && zuneBase.toString(16)}`);
console.log('-- by task:', [...tasks].sort((a, b) => b[1] - a[1]).slice(0, 6).map(([k, v]) => k + ' ' + (100 * v / tot).toFixed(1) + '%').join(' | '));
console.log('-- by PC');
for (const [k, v] of [...counts].sort((a, b) => b[1] - a[1]).slice(0, top)) console.log(String(v).padStart(5), (100 * v / tot).toFixed(1).padStart(5) + '%', k);
console.log('-- ROM samples by Zune caller (nearest return address)');
for (const [k, v] of [...byCaller].filter(e => e[0].startsWith('via')).sort((a, b) => b[1] - a[1]).slice(0, top)) console.log(String(v).padStart(5), (100 * v / tot).toFixed(1).padStart(5) + '%', k);
