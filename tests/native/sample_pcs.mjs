// sample_pcs.mjs <out.json> <seconds>: sample the PC of a running WinUAE guest over its gdbserver (PORT, default 2355) every 15 ms, with 24 stack longwords, the task name/state and the library jump tables; see PROFILING.phases.md
// GdbProtocol comes from mcp-winuae-emu: set WINUAE_GDB_PROTOCOL to its
// dist/gdb-protocol.js.
const {GdbProtocol} = await import(process.env.WINUAE_GDB_PROTOCOL ||
    '../../../mcp-winuae-emu/dist/gdb-protocol.js');
import {writeFileSync} from 'node:fs';
const p = new GdbProtocol(); await p.connect('127.0.0.1', Number(process.env.PORT || 2355));
const u32 = async a => (await p.readMemory(a, 4)).readUInt32BE(0);
const str = async a => (await p.readMemory(a, 40)).toString('latin1').split('\0')[0];
const out = {libs: [], samples: []};
try {
  await p.pause();
  const sys = await u32(4);
  let n = await u32(sys + 378);
  const libs = [['exec.library', sys]];
  while (await u32(n)) { libs.push([await str(await u32(n + 10)), n]); n = await u32(n); }
  for (const [name, base] of libs) {
    if (!/exec|intuition|graphics|layers|utility|dos\.|gadtools|diskfont|keymap|commodities|muimaster|timer/.test(name)) continue;
    const neg = (await p.readMemory(base + 16, 2)).readUInt16BE(0);
    const vectors = [];
    for (let lvo = 6; lvo < neg; lvo += 6) {
      const j = await p.readMemory(base - lvo, 6);
      if (j.readUInt16BE(0) === 0x4ef9) vectors.push([lvo, j.readUInt32BE(2)]);
    }
    out.libs.push({name, base, vectors});
  }
  await p.continue();
  const end = Date.now() + 1000 * Number(process.argv[3] || 60);
  while (Date.now() < end) {
    await new Promise(r => setTimeout(r, 15));
    await p.pause();
    const pc = await p.readRegister(17);
    const sp = await p.readRegister(15);
    // return address candidates: first 24 longwords on the stack
    const stack = await p.readMemory(sp, 96);
    const ret = []; for (let i = 0; i < 24; i++) ret.push(stack.readUInt32BE(i * 4));
    const task = await u32(sys + 276);
    const tname = await str(await u32(task + 10));
    const tstate = (await p.readMemory(task + 15, 1))[0];
    out.samples.push([pc, ret, tname, tstate]);
    await p.continue();
  }
  // dump the hottest RAM code pages (outside ROM) for offline disassembly
  const pages = new Map();
  for (const [pc] of out.samples) if (!(pc >= 0xf80000 && pc < 0x1000000)) pages.set(pc & ~0x3ff, (pages.get(pc & ~0x3ff) || 0) + 1);
  out.mem = {};
  await p.pause();
  for (const [page] of [...pages].sort((a, b) => b[1] - a[1]).slice(0, 6)) out.mem[page.toString(16)] = (await p.readMemory(page, 0x400)).toString('hex');
} finally { try { await p.continue(); } catch {} await p.detach(); }
writeFileSync(process.argv[2], JSON.stringify(out));
console.log('samples', out.samples.length);
