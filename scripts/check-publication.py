"""Check committed publication surfaces and pinned upstream source notices."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVES = {
    'third_party/background-engines/lite-source.zip': 'd6da69e58e90ea7a14810444f6dddcca73d2fd8ee59bb71943e1a12a2ff15041',
    'third_party/background-engines/advanced-source.zip': '4790c950f7f9aa441c1691da46755300dcf3839ed8c205a0df1ac34bcfe8c7e9',
    'third_party/video-matting/rvm-source.zip': 'b58b1187fb9373f56db2e463a7629cf9f539c4c97ea361c793620396211122da',
    'third_party/video-matting/bmv2-source.zip': 'f8bd03b63cebd6edbad860ce18b8422392dd330bad9216232b0808ea6566e906',
}
NOTICES = ['LICENSE', 'LICENSES/GPL-2.0.txt', 'LICENSES/GPL-3.0.txt',
           'CREDITS.md', 'THIRD-PARTY-NOTICES.md', 'docs/LICENSING.md',
           'docs/DEPENDENCY-SOURCES.md', 'third_party/face-beautifier/LICENSE',
           'third_party/face-tracking/LICENSE-LUMINA.txt', 'models/LICENSE-Apache-2.0.txt',
           'third_party/video-matting/data/licenses/DirectML-LICENSE.txt']
PRIVATE_PATH = re.compile(r'(^|/)(?:\.runtime|work|dist|global-home|global-homes)(?:/|$)|'
                          r'(^|/)(?:\.env(?:\.[^/]*)?|[^/]*\.veilbg|trueitem-user-[^/]*\.json)$', re.I)
TEXT_PATTERNS = {
    'private_key': re.compile(rb'-----BEGIN (?:OPENSSH|RSA|EC|DSA) PRIVATE KEY-----'),
    'credential_signature': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{45,})\b'),
    'windows_user_path': re.compile(rb'(?i)[a-z]:[\\/]Users[\\/][^\\/\s"<>]+'),
    'private_surface_marker': re.compile(rb'NOT\s+public.safe'),
}


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    findings = []
    for name in NOTICES:
        if not (ROOT / name).is_file():
            findings.append({'kind': 'missing_notice', 'path': name})
    for name, expected in ARCHIVES.items():
        p = ROOT / name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != expected:
            findings.append({'kind': 'source_archive_hash', 'path': name})
            continue
        with zipfile.ZipFile(p) as z:
            if z.testzip() is not None:
                findings.append({'kind': 'source_archive_integrity', 'path': name})
            for member in z.namelist():
                if member.startswith(('/', '\\')) or '..' in Path(member).parts:
                    findings.append({'kind': 'archive_path_escape', 'path': name})
    # Read only reachable Git objects, never local profiles, keys, photos or logs.
    commits = git('rev-list', '--all').decode().splitlines()
    objects = {}
    for commit in commits:
        for entry in git('ls-tree', '-r', '-z', '--full-tree', commit).split(b'\0'):
            if not entry:
                continue
            metadata, raw_path = entry.split(b'\t', 1)
            mode, kind, oid = metadata.split()
            path = raw_path.decode('utf-8')
            if PRIVATE_PATH.search(path) or path.startswith(('runtime/', 'archive/global-homes/')):
                findings.append({'kind': 'private_committed_path', 'commit': commit, 'path': path})
            if mode == b'160000':
                findings.append({'kind': 'submodule_requires_separate_check', 'path': path})
            if kind == b'blob':
                objects.setdefault(oid.decode(), set()).add(path)
    scanned = 0
    for oid, paths in objects.items():
        data = git('cat-file', 'blob', oid)
        if data.startswith(b'version https://git-lfs.github.com/spec/'):
            findings.append({'kind': 'lfs_requires_separate_check', 'path': sorted(paths)[0]})
        if b'\0' in data[:8192]:
            continue
        scanned += 1
        for kind, pattern in TEXT_PATTERNS.items():
            match = pattern.search(data)
            if match:
                findings.append({'kind': kind, 'path': sorted(paths)[0],
                                 'line': data.count(b'\n', 0, match.start()) + 1})
    # Check source/notice links in the current checkout, not external web content.
    for name in ['README.md', 'CREDITS.md', 'THIRD-PARTY-NOTICES.md',
                 'docs/LICENSING.md', 'docs/DEPENDENCY-SOURCES.md', 'docs/BUILD.md']:
        p = ROOT / name
        if not p.is_file():
            continue
        for link in re.findall(r'\]\(([^)]+)\)', p.read_text(encoding='utf-8-sig')):
            if '://' in link or link.startswith('#'):
                continue
            target = link.split('#', 1)[0]
            if not (p.parent / target).exists():
                findings.append({'kind': 'broken_local_link', 'path': name, 'target': target})
    report = {'status': 'PASS' if not findings else 'FAIL', 'reachable_commits': len(commits),
              'unique_blobs': len(objects), 'text_blobs_scanned': scanned,
              'source_archives': len(ARCHIVES), 'findings': findings,
              'limits': 'Checks committed paths/text and known upstream archive hashes. '
                        'Not a malware audit, legal certification or dangling-object scan.'}
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if not findings else 1


if __name__ == '__main__':
    sys.exit(main())
