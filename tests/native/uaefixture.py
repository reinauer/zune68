"""Boot a WinUAE fixture that runs Work:stage, and watch files in its Work:.

A fixture is a directory holding run.uae and work/. run.uae mounts work/ as
Work: and boots a system whose Startup-Sequence ends with "Execute
Work:stage" (the WinUAE fixture kit's mkfx.sh makes such directories). The
host writes work/stage, boots, and follows the files the guest writes into
work/. Only this fixture's emulator is ever stopped: other WinUAE instances
on the machine are left alone.
"""
import os
import shutil
import subprocess
import time

WINUAE = os.environ.get('WINUAE', '/Applications/WinUAE.app/Contents/MacOS/WinUAE')


class Fixture:
    def __init__(self, path, emulator=WINUAE):
        self.dir = os.path.realpath(path)
        self.uae = os.path.join(self.dir, 'run.uae')
        self.work = os.path.join(self.dir, 'work')
        self.emulator = emulator
        self.proc = None
        if not os.path.isfile(self.uae) or not os.path.isdir(self.work):
            raise SystemExit('%s: no run.uae or work/' % self.dir)
        # pkill/pgrep pattern: matches only the emulator started with this run.uae
        self.pattern = '%s -f %s' % (os.path.basename(emulator), self.uae)

    def port(self):
        for line in open(self.uae):
            if line.startswith('gdb_port='):
                return int(line.split('=', 1)[1])
        return None

    def running(self):
        return subprocess.run(['pgrep', '-f', self.pattern], stdout=subprocess.DEVNULL,
                              check=False).returncode == 0

    def stop(self):
        if self.proc:
            self.proc.kill()
            self.proc.wait()
            self.proc = None
        for _ in range(20):
            if not self.running():
                return
            subprocess.run(['pkill', '-f', self.pattern], check=False)
            time.sleep(0.5)
        raise RuntimeError('emulator of %s does not stop' % self.dir)

    def boot(self, stage):
        """Write Work:stage (a list of AmigaDOS lines) and start the emulator."""
        self.stop()
        with open(os.path.join(self.work, 'stage'), 'w', encoding='latin-1') as f:
            f.write('\n'.join(stage) + '\n')
        log = open(os.path.join(self.dir, 'emu.log'), 'w')
        self.proc = subprocess.Popen([self.emulator, '-f', self.uae],
                                     stdout=log, stderr=subprocess.STDOUT)
        log.close()

    def read(self, rel):
        path = os.path.join(self.work, rel)
        try:
            with open(path, encoding='latin-1') as f:
                return f.read()
        except OSError:
            return ''

    def wait(self, rel, finished, stall, first=180):
        """Poll Work:<rel> until finished(text) is true. Returns (text, why):
        why is 'done', 'stall' (no change for `stall` seconds, or nothing
        written within `first` seconds of booting) or 'exited'."""
        text, since = None, time.time()
        while True:
            now = self.read(rel)
            if now != text:
                text, since = now, time.time()
            if finished(text):
                return text, 'done'
            if self.proc and self.proc.poll() is not None:
                return text, 'exited'
            if time.time() - since > (stall if text else first):
                return text, 'stall'
            time.sleep(1)


def stage_zune(fix, libdir):
    """Copy a Zune68 build (muimaster.library, Libs/MUI) to Work:libs and
    Work:mui/Libs/MUI. Returns the stage lines that put them first on LIBS:
    and MUI:."""
    libs = os.path.join(fix.work, 'libs')
    classes = os.path.join(fix.work, 'mui', 'Libs', 'MUI')
    for d in (libs, classes):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    shutil.copy(os.path.join(libdir, 'muimaster.library'), libs)
    src = os.path.join(libdir, 'Libs', 'MUI')
    for f in os.listdir(src):
        if not f.endswith('.map'):
            shutil.copy(os.path.join(src, f), classes)
    return ['Assign MUI: Work:mui', 'Assign LIBS: Work:libs',
            'Assign LIBS: SYS:Libs ADD', 'Assign LIBS: MUI:Libs ADD']
