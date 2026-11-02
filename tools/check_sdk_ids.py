#!/usr/bin/env python3
"""Compare the values of MUI identifiers between an original SDK and Zune68.

Every object-like MUIA_, MUIM_, MUIV_, MUIC_, MUII_, MUIX_, MUIKEY_, MUIE_,
MUIO_, MUIPEN_, MUICFG_ and MCC_ macro that the SDK's libraries/mui.h or
mui/<Class>_mcc.h defines is expanded with both header sets by the cross
preprocessor. A name both define with different values is an ABI error,
because binaries built with the SDK pass the SDK's numbers.
"""
import argparse
import ast
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r'(?:MUI[AMVCIXEO]|MUIKEY|MUIPEN|MUICFG|MCC)_\w+'
DEFINE = re.compile(r'^\s*#\s*define\s+(' + PREFIX + r')(?![\w(])', re.M)
CAST = re.compile(r'\(\s*(?:const\s+)?(?:unsigned\s+long|unsigned\s+int|long|int|'
                  r'U?LONG|U?WORD|U?BYTE|IPTR|SIPTR|Tag|STRPTR|CONST_STRPTR|APTR|'
                  r'ULONG\s*\*|char\s*\*)\s*\)')
ZUNE_INCLUDES = [
    'workbench/libs/muimaster/include',
    'workbench/classes/zune/nlist/include',
    'workbench/classes/zune/betterstring/include',
    'workbench/classes/zune/texteditor/include',
    'workbench/classes/zune/thebar/include',
]


def headers(sdk_include):
    found = [Path('libraries/mui.h')]
    found += sorted(p.relative_to(sdk_include)
                    for p in (sdk_include / 'mui').glob('*_mcc.h'))
    return found


def zune_header(name):
    """Zune68's header for an SDK MCC header, or None."""
    base = Path(name).name
    for path in sorted((ROOT / 'workbench/classes/zune').rglob(base)):
        return path
    return None


def expand(cc, includes, files, names):
    source = ''.join(f'#include "{f}"\n' for f in files)
    source = ('#include <exec/types.h>\n#include <utility/tagitem.h>\n'
              '#include <libraries/mui.h>\n' + source)
    for index, name in enumerate(names):
        source += f'#ifdef {name}\n@@{index}@@ = {name}\n#endif\n'
    with tempfile.TemporaryDirectory() as tmp:
        c = Path(tmp) / 'ids.c'
        c.write_text(source)
        cmd = [cc, '-E', '-P', '-w', '-D__amigaos3__']
        for inc in includes:
            cmd += ['-I', str(inc)]
        result = subprocess.run(cmd + [str(c)], capture_output=True, text=True)
    if result.returncode:
        sys.exit(f'{cc} -E failed:\n{result.stderr}')
    values = {}
    for line in result.stdout.splitlines():
        m = re.match(r'@@(\d+)@@ = (.*)$', line)
        if m:
            values[names[int(m.group(1))]] = value(m.group(2))
    return values


def value(text):
    text = CAST.sub('', text).strip()
    if '"' in text:
        try:
            return ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', text))
        except (ValueError, SyntaxError):
            return text
    expr = re.sub(r'\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b', r'\1', text)
    expr = expr.replace('||', ' or ').replace('&&', ' and ')
    try:
        number = eval(expr, {'__builtins__': {}}, {})
        return number & 0xffffffff if isinstance(number, int) else text
    except Exception:
        return ' '.join(text.split())


def show(v):
    return f'0x{v:08x}' if isinstance(v, int) else repr(v)


def compare(sdk_include, build_include, cc='m68k-amigaos-gcc', list_missing=False):
    """Print the differences; return the number of differing values."""
    sdk = Path(sdk_include).resolve()
    zune_inc = [Path(build_include).resolve()] + [ROOT / p for p in ZUNE_INCLUDES]
    errors = missing = shared = 0
    for header in headers(sdk):
        names = sorted(set(DEFINE.findall((sdk / header).read_text(errors='replace'))))
        if not names:
            continue
        ours = header if header.name == 'mui.h' else zune_header(header)
        if ours is None:
            missing += len(names)
            if list_missing:
                print(f'{header}: no Zune68 header for {len(names)} names')
            continue
        theirs = expand(cc, [sdk], [sdk / header], names)
        mine = expand(cc, zune_inc + ([ours.parent] if ours != header else []),
                      [] if ours == header else [ours], names)
        absent = [n for n in names if n in theirs and n not in mine]
        missing += len(absent)
        for name in names:
            if name in theirs and name in mine:
                shared += 1
                if theirs[name] == mine[name]:
                    continue
                # Counts and last-pattern markers describe what each library
                # implements; they are not passed as identifiers.
                marker = name.endswith('_Count') or name == 'MUII_LASTPAT'
                errors += not marker
                print(f'{header}: {name} SDK {show(theirs[name])} '
                      f'Zune68 {show(mine[name])}' + (' (range marker)' if marker else ''))
        if absent and list_missing:
            print(f'{header}: not in Zune68: {" ".join(absent)}')
    print(f'{shared} shared identifiers compared, {errors} differ, '
          f'{missing} SDK identifiers not defined by Zune68')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-include', type=Path, required=True,
                        help='the SDK C include directory (holds libraries/mui.h)')
    parser.add_argument('--build-include', type=Path, required=True,
                        help='the generated Zune68 include directory')
    parser.add_argument('--cc', default='m68k-amigaos-gcc')
    parser.add_argument('--missing', action='store_true',
                        help='also list SDK identifiers that Zune68 does not define')
    args = parser.parse_args()
    return 1 if compare(args.sdk_include, args.build_include, args.cc, args.missing) else 0


if __name__ == '__main__':
    sys.exit(main())
