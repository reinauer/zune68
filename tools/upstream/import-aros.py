#!/usr/bin/env python3
"""Extract reproducible Zune history into a NEW repository.

Run with Python from an environment containing git-filter-repo==2.47.0.
The source repository is read only. The output path must not exist.
To update Zune68, extract again with the same recipe, fetch its
aros-upstream branch, check that it extends the previous baseline, and merge.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shutil
import subprocess
import sys

from filter_submodules import filter_submodules

HERE = Path(__file__).resolve().parent
VERSION = '2.47.0'


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def entries(repo, revision):
    result = {}
    for entry in git(repo, 'ls-tree', '-rz', revision).split(b'\0'):
        if entry:
            meta, name = entry.split(b'\t', 1)
            result[name] = meta
    return result


def verify(source, revision, target, paths):
    prefixes = tuple(p.encode() for p in paths if p.endswith('/'))
    exact = {p.encode() for p in paths if not p.endswith('/')}
    expected = {p: v for p, v in entries(source, revision).items()
                if p in exact or p.startswith(prefixes)}
    actual = entries(target, 'aros-upstream')
    if b'.gitmodules' in expected:
        data = filter_submodules(git(source, 'show', revision + ':.gitmodules'))
        oid = subprocess.check_output(['git', 'hash-object', '--stdin'],
                                      input=data).strip()
        expected[b'.gitmodules'] = b'100644 blob ' + oid
    if actual != expected:
        differences = sorted(p.decode(errors='replace')
                             for p in actual.keys() | expected.keys()
                             if actual.get(p) != expected.get(p))
        raise RuntimeError('Extracted tree mismatch: ' + ', '.join(differences))
    return len(actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if output.exists():
        parser.error('output must be a new directory')
    if importlib.metadata.version('git-filter-repo') != VERSION:
        parser.error('install git-filter-repo==' + VERSION)
    if git(source, 'rev-parse', '--is-shallow-repository').strip() != b'false':
        parser.error('source must have complete history')
    revision = git(source, 'rev-parse', '--verify',
                   args.revision + '^{commit}').decode().strip()
    paths = (HERE / 'paths.txt').read_text().splitlines()
    subprocess.run(['git', 'init', '--initial-branch=aros-upstream', str(output)],
                   check=True)
    # Fetch objects rather than using shared storage or hard links.
    subprocess.run(['git', '-C', str(output), 'fetch', '--no-tags',
                    str(source), revision], check=True)
    subprocess.run(['git', '-C', str(output), 'update-ref',
                    'refs/heads/aros-upstream', 'FETCH_HEAD'], check=True)
    callback = (HERE / 'filter_submodules.py').read_text() + '''
if filename == b'.gitmodules':
    contents = value.get_contents_by_identifier(blob_id)
    blob_id = value.insert_file_with_contents(filter_submodules(contents))
return (filename, mode, blob_id)
'''
    # The module's main() is not run when importing git_filter_repo.
    command = [sys.executable, '-c',
               'import git_filter_repo; git_filter_repo.main()',
               '--force', '--paths-from-file', str(HERE / 'paths.txt'),
               '--preserve-commit-hashes', '--preserve-commit-encoding',
               '--file-info-callback', callback,
               '--commit-callback',
               "commit.message = commit.message.rstrip(b'\\n') + "
               "b'\\n\\nAROS-Commit: ' + commit.original_id + b'\\n'"]
    subprocess.run(command, cwd=output, check=True)
    file_count = verify(source, revision, output, paths)
    filtered = git(output, 'rev-parse', 'aros-upstream').decode().strip()
    commits = int(git(output, 'rev-list', '--count', 'aros-upstream'))
    report_dir = output / '.git/filter-repo'
    recipe_hashes = {name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
                     for name in ['paths.txt', 'filter_submodules.py',
                                  'import-aros.py']}
    report = {
        'source_url': 'https://github.com/aros-development-team/AROS.git',
        'source_revision': revision,
        'filtered_revision': filtered,
        'git_filter_repo_version': VERSION,
        'git_filter_repo_id': 'a40bce548d2c',
        'recipe_sha256': recipe_hashes,
        'retained_commits': commits,
        'tree_entries': file_count,
        'tree_check': 'Selected paths match source; .gitmodules is scoped.',
    }
    (report_dir / 'source.json').write_text(json.dumps(report, indent=2) + '\n')
    shutil.copyfile(HERE / 'paths.txt', report_dir / 'paths.txt')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
