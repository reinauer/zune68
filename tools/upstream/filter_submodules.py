"""Keep complete .gitmodules sections for the extracted components."""
import re


def filter_submodules(contents):
    sections = re.split(br'(?m)(?=^\[)', contents)
    kept = []
    for section in sections:
        match = re.search(br'(?m)^\s*path\s*=\s*(.*?)\s*$', section)
        if match and match.group(1).startswith((
            b'workbench/libs/muimaster/',
            b'workbench/libs/muiscreen/',
            b'workbench/classes/zune/',
            b'workbench/prefs/Zune/',
        )):
            kept.append(section)
    return b''.join(kept)
