#!/usr/bin/env python3
"""Write the native library envelope of an AROS class from its .conf file."""
import argparse
from pathlib import Path
import re

# Libraries a class reaches through <proto/...> that mccinit.c does not open.
BASES = {'locale': ('struct LocaleBase', 'LocaleBase', 'locale.library', True),
         'icon': ('struct Library', 'IconBase', 'icon.library', True),
         'asl': ('struct Library', 'AslBase', 'asl.library', True),
         'cybergraphics': ('struct Library', 'CyberGfxBase', 'cybergraphics.library', False)}


def section(text, name):
    match = re.search(rf'##begin {name}\n(.*?)##end {name}', text, re.S)
    return match[1] if match else ''


def config(text):
    return dict(line.split(None, 1) for line in section(text, 'config').splitlines()
                if len(line.split(None, 1)) == 2)


def methods(text):
    return [line.split()[0] for line in section(text, 'methodlist').splitlines()
            if line.strip()]


def dispatcher(name, base, entries):
    cases = ''.join(f'    case {m}: return (IPTR){base}__{m}(cl, (APTR)obj, (APTR)msg);\n'
                    for m in entries)
    return (f'DISPATCHER({name})\n{{\n    switch (msg->MethodID)\n    {{\n{cases}'
            '    }\n    return DoSuperMethodA(cl, obj, msg);\n}\n')


# Soft-float code from libm calls these; libnix's startup does not run here.
MATH = [('struct Library', 'MathIeeeDoubBasBase', 'mathieeedoubbas.library', True),
        ('struct Library', 'MathIeeeSingBasBase', 'mathieeesingbas.library', True),
        ('struct Library', 'MathIeeeDoubTransBase', 'mathieeedoubtrans.library', True)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('conf', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    text = args.conf.read_text(encoding='latin-1')
    private = re.search(r'##begin class\n(.*?)##end class', text, re.S)
    main_text = text[:private.start()] + text[private.end():] if private else text
    conf = config(main_text)
    base = conf['basename']
    version, revision = conf['version'].split('.')
    directory = args.conf.parent
    sources = sorted(p.name for p in directory.glob('*.c'))
    inits, expunges, opened = [], [], []
    for source in sources:
        code = (directory / source).read_text(encoding='latin-1')
        opened += [BASES[n] for n in re.findall(r'<proto/(\w+)\.h>', code)
                   if n in BASES and BASES[n] not in opened]
        if re.search(r'\b(float|double)\b', code):
            opened += [base for base in MATH if base not in opened]
        inits += re.findall(r'ADD2INITLIB\(\s*(\w+)', code)
        expunges += re.findall(r'ADD2EXPUNGELIB\(\s*(\w+)', code)
    lines = [f'/* Generated from {args.conf.name}. */', '#include "class_common.h"']
    lines += [f'{kind} *{name};' for kind, name, _, _ in opened]
    lines += [f'#include "{source}"' for source in sources]
    lines += ['#undef VERSION', '#undef REVISION',
              f'#define CLASS MUIC_{base}', f'#define SUPERCLASS {conf["superclass"]}',
              f'#define INSTDATA {conf["classdatatype"].removeprefix("struct ")}',
              f'#define USERLIBID CLASS " {conf["version"]} ({conf["date"]}) '
              'AROS native AmigaOS port"',
              f'#define VERSION {version}', f'#define REVISION {revision}',
              '#define MASTERVERSION 19', '#define MIN_STACKSIZE 16384',
              '#define SetupDebug() ((void)0)', '#define CleanupDebug() ((void)0)']
    hook = bool(inits or expunges or private or opened)
    if hook:
        lines += ['#define CLASSINIT', '#define CLASSEXPUNGE',
                  'static BOOL ClassInit(struct Library *base);',
                  'static void ClassExpunge(struct Library *base);']
    lines.append('#include "mccinit.c"')
    if private:
        extra = config(private[1])
        lines += ['', dispatcher('_IconDispatcher', extra['basename'], methods(private[1]))]
    lines += ['', dispatcher('_Dispatcher', base, methods(main_text))]
    if hook:
        body, tail = [], []
        for kind, name, library, required in opened:
            body.append(f'    {name} = ({kind} *)OpenLibrary("{library}", 0);')
            if required:
                body.append(f'    if (!{name}) return FALSE;')
            tail.insert(0, f'    if ({name}) CloseLibrary((struct Library *){name});')
            tail.insert(1, f'    {name} = NULL;')
        body += [f'    if (!{name}(base)) return FALSE;' for name in inits]
        tail = [f'    {name}(base);' for name in reversed(expunges)] + tail
        if private:
            extra = config(private[1])
            ptr, kind = extra['classptr_var'], extra['classdatatype'].removeprefix('struct ')
            body += [f'    {ptr} = MUI_CreateCustomClass(NULL, {extra["superclass"]}, NULL, '
                     f'sizeof(struct {kind}), ENTRY(_IconDispatcher));',
                     f'    if (!{ptr}) return FALSE;']
            tail.insert(0, f'    if ({ptr}) MUI_DeleteCustomClass({ptr});')
        lines += ['static BOOL ClassInit(struct Library *base)\n{', *body,
                  '    return TRUE;\n}', '',
                  'static void ClassExpunge(struct Library *base)\n{', *tail, '}']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
