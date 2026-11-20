#!/usr/bin/env python3
"""Download the real-program archives from Aminet and unpack them as data.

  fetch.py [--cache DIR] [--ids ID,ID...] [--group mui3] [--list]

The archives named in programs.tsv (the programs) and support.tsv (the
classes and libraries they use) go to DIR/<key>/ (default
~/.cache/zune68-real), each unpacked into a new directory DIR/<key>/x.
They are third-party software: never commit or redistribute them. Only
Aminet (aminet.net and its mirrors) is contacted; archives from elsewhere
are listed as "not on Aminet" and must be put into DIR/<key>/ by hand.

Downloaded archives are untrusted. They are only listed and unpacked with
lha (lzx archives with unlzx, if installed); nothing in them is run on the
host. An archive with absolute paths, ".." or links in it is not unpacked.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.expanduser('~/.cache/zune68-real')
AMINET = 'https://aminet.net/'
AGENT = 'zune68-real-fetch/1'
# Amiga archives name files in Latin-1
LHA_NAMES = ['--archive-kanji-code=latin1', '--system-kanji-code=utf8']


def read_tsv(name):
    rows = []
    with open(os.path.join(HERE, name), encoding='utf-8') as f:
        head = f.readline().lstrip('# ').rstrip('\n').split('\t')
        for line in f:
            if line.strip() and not line.startswith('#'):
                rows.append(dict(zip(head, line.rstrip('\n').split('\t'))))
    return rows


def programs():
    return read_tsv('programs.tsv')


def support():
    return read_tsv('support.tsv')


def archive_name(archive):
    return urllib.parse.unquote(archive.rsplit('/', 1)[-1])


def on_aminet(archive):
    return bool(archive) and '://' not in archive


def download(archive, dest, mirror):
    url = mirror + archive
    req = urllib.request.Request(url, headers={'User-Agent': AGENT})
    with urllib.request.urlopen(req, timeout=120) as r:
        host = urllib.parse.urlparse(r.geturl()).hostname or ''
        if host != 'aminet.net' and not host.endswith('.aminet.net'):
            raise RuntimeError('redirected away from Aminet: %s' % r.geturl())
        data = r.read()
    tmp = dest + '.part'
    with open(tmp, 'wb') as f:
        f.write(data)
    os.replace(tmp, dest)


def lha_names(arc):
    """Member names of an lha archive, from lha's verbose listing."""
    r = subprocess.run(['lha', '-vq'] + LHA_NAMES + [arc], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, check=False)
    if r.returncode:
        raise RuntimeError('lha cannot list %s' % arc)
    names = []
    for line in r.stdout.decode('latin-1').splitlines():
        # ... -lh5- ec67 Aug  9 15:03 NAME: the name follows method, CRC and date
        m = re.search(r' -(?:l[hz][0-9a-z]|pm[0-9s])- +\S+ +\S+ +\S+ +\S+ (.*)$', line)
        if not m:
            raise RuntimeError('unexpected lha listing line %r' % line)
        names.append(m.group(1))
    return names


def safe(names):
    for n in names:
        parts = n.replace('\\', '/').split('/')
        if n.startswith('/') or '..' in parts or ' -> ' in n or ':' in parts[0]:
            return n
    return None


def unpack(arc, into):
    """Unpack arc into the new directory `into` (removed first if it exists)."""
    if arc.lower().endswith('.lzx'):
        if not shutil.which('unlzx'):
            raise RuntimeError('no unlzx on PATH for %s' % arc)
        tool = ['unlzx', '-x', os.path.abspath(arc)]
    else:
        bad = safe(lha_names(arc))
        if bad:
            raise RuntimeError('unsafe member %r in %s' % (bad, arc))
        tool = ['lha', '-xqw=' + into] + LHA_NAMES + [os.path.abspath(arc)]
    shutil.rmtree(into, ignore_errors=True)
    os.makedirs(into)
    r = subprocess.run(tool, cwd=into, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       check=False)
    # lha complains about directory entries whose directory it made already
    errors = [l for l in r.stdout.decode('utf-8', 'replace').splitlines()
              if l.strip() and not l.rstrip().endswith('/"')]
    if r.returncode and errors:
        raise RuntimeError('unpacking %s failed: %s' % (arc, ' '.join(errors)[-300:]))
    # an archive holding only files and one more lha/lzh: that one goes to x/inner
    inner = [f for f in os.listdir(into) if f.lower().endswith(('.lha', '.lzh'))]
    subdirs = [f for f in os.listdir(into) if os.path.isdir(os.path.join(into, f))]
    if len(inner) == 1 and not subdirs:
        unpack(os.path.join(into, inner[0]), os.path.join(into, 'inner'))


def fetch(key, archive, cache, mirror, refresh=False):
    """Make sure DIR/key/x exists. Returns a status word."""
    d = os.path.join(cache, key)
    arc = os.path.join(d, archive_name(archive)) if archive else None
    if os.path.isdir(os.path.join(d, 'x')) and not refresh:
        return 'cached'
    os.makedirs(d, exist_ok=True)
    if not arc or not os.path.exists(arc):
        if not on_aminet(archive):
            here = [f for f in os.listdir(d) if f.lower().endswith(('.lha', '.lzh', '.lzx'))]
            if not here:
                return 'not on Aminet'
            arc = os.path.join(d, here[0])
        else:
            download(archive, arc, mirror)
    unpack(arc, os.path.join(d, 'x'))
    return 'unpacked'


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--cache', default=CACHE)
    p.add_argument('--ids', help='comma-separated program ids (default: all)')
    p.add_argument('--group', help='only programs of this group (mui3, mui5, mcc)')
    p.add_argument('--no-support', action='store_true',
                   help='skip the archives of support.tsv')
    p.add_argument('--mirror', default=AMINET, help='Aminet mirror URL')
    p.add_argument('--refresh', action='store_true', help='unpack again')
    p.add_argument('--list', action='store_true', help='only list what would be fetched')
    a = p.parse_args()
    rows = programs()
    if a.ids:
        want = a.ids.split(',')
        unknown = set(want) - {r['id'] for r in rows}
        if unknown:
            p.error('unknown ids: %s' % ', '.join(sorted(unknown)))
        rows = [r for r in rows if r['id'] in want]
    if a.group:
        rows = [r for r in rows if r['group'] == a.group]
    keys = {}
    for r in rows + ([] if a.no_support else support()):
        keys.setdefault(r['key'], r['archive'])
    if not a.mirror.endswith('/'):
        a.mirror += '/'
    failed = 0
    for key, archive in sorted(keys.items()):
        if a.list:
            print('%-16s %s' % (key, archive or '-'))
            continue
        try:
            status = fetch(key, archive, a.cache, a.mirror, a.refresh)
        except Exception as e:  # report and go on with the next archive
            status = 'failed: %s' % e
            failed += 1
        print('%-16s %-40s %s' % (key, archive_name(archive) if archive else '-', status),
              flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
