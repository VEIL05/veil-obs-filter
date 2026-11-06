"""Fetch pinned official build inputs into this project only."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work' / 'build-tools'
DOWNLOADS = WORK / 'downloads'
DOWNLOADS.mkdir(parents=True, exist_ok=True)
OBS_SOURCE_SHA256 = '0f228b960c099c6f6bca9f09a0c3516f14fdf1dd96e1d4702145c5d3ba8cba31'


def verify_obs_source(archive_path=None, sdk_path=None):
    """Verify prepared SDK bytes against the pinned archive, without extraction."""
    archive_path = archive_path or DOWNLOADS / 'obs-studio-33.0.1.zip'
    sdk_path = (sdk_path or ROOT / 'third_party/obs-sdk/obs-studio-33.0.1').resolve()
    if not sdk_path.is_relative_to(ROOT.resolve()):
        raise RuntimeError('Prepared OBS SDK source directory escapes this checkout')
    if hashlib.sha256(archive_path.read_bytes()).hexdigest() != OBS_SOURCE_SHA256:
        raise RuntimeError('OBS source archive SHA256 mismatch')
    prefix = 'obs-studio-33.0.1/'
    with zipfile.ZipFile(archive_path) as archive:
        members = {member.filename[len(prefix):]: member for member in archive.infolist()
                   if not member.is_dir() and member.filename.startswith(prefix)}
        if not members:
            raise RuntimeError('OBS source archive has no SDK members')
        actual = {path.relative_to(sdk_path).as_posix(): path
                  for path in sdk_path.rglob('*') if path.is_file()}
        if set(actual) != set(members):
            raise RuntimeError('Prepared OBS SDK file names differ from the pinned archive')
        fingerprint = hashlib.sha256()
        for name in sorted(members):
            path = actual[name]
            if not path.resolve().is_relative_to(sdk_path):
                raise RuntimeError('Prepared OBS SDK path escapes its source directory')
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != hashlib.sha256(archive.read(members[name])).hexdigest():
                raise RuntimeError('Prepared OBS SDK bytes differ: ' + name)
            fingerprint.update((name + '\0' + digest + '\n').encode('utf-8'))
    return {'status': 'verified', 'files': len(members),
            'source_fingerprint': fingerprint.hexdigest(), 'archive_sha256': OBS_SOURCE_SHA256}


def request(url):
    return urllib.request.urlopen(urllib.request.Request(
        url, headers={'User-Agent':'TRUEITEM-local-OBS-build'}), timeout=120)


def fetch(url, path, size=None, sha256=None):
    if not path.exists():
        with request(url) as source, path.open('wb') as output:
            while block := source.read(1024 * 1024):
                output.write(block)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if size is not None and path.stat().st_size != size:
        raise RuntimeError('Download size mismatch: '+path.name)
    if sha256 is not None and digest != sha256:
        raise RuntimeError('Download SHA256 mismatch: '+path.name)
    return {'url':url, 'file':str(path), 'bytes':path.stat().st_size,
            'sha256':digest, 'sha256_matches_pinned_input':bool(sha256)}


def extract(path, target):
    target = target.resolve()
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path) as archive:
        for member in archive.infolist():
            dest = (target / member.filename).resolve()
            if target != dest and target not in dest.parents:
                raise RuntimeError('Unsafe zip member')
        archive.extractall(target)


def main():
    with request('https://api.github.com/repos/mstorsjo/llvm-mingw/releases/tags/20261020') as source:
        release = json.load(source)
    assets = [a for a in release['assets'] if a['name'].endswith('ucrt-x86_64.zip')]
    if len(assets) != 1:
        raise RuntimeError('Expected unique native x64 UCRT toolchain')
    asset = assets[0]
    if asset['name'] != 'llvm-mingw-20261020-ucrt-x86_64.zip':
        raise RuntimeError('Pinned toolchain asset name changed')
    expected = asset.get('digest','')
    if not expected.startswith('sha256:'):
        raise RuntimeError('Official asset has no SHA256 digest')
    if expected[7:] != 'fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd':
        raise RuntimeError('Pinned toolchain digest changed')
    sdk_url = 'https://codeload.github.com/obsproject/obs-studio/zip/refs/tags/33.0.1'
    jobs = [
        (asset['browser_download_url'], DOWNLOADS/asset['name'], asset['size'], expected[7:]),
        (sdk_url, DOWNLOADS/'obs-studio-33.0.1.zip', None,
         OBS_SOURCE_SHA256),
    ]
    with ThreadPoolExecutor(max_workers=2) as pool:
        futures = [pool.submit(fetch,*job) for job in jobs]
        results = [f.result() for f in futures]
    print('Downloads verified', flush=True)
    extract(Path(results[0]['file']), ROOT/'third_party/toolchain')
    print('Toolchain extracted', flush=True)
    extract(Path(results[1]['file']), ROOT/'third_party/obs-sdk')
    manifest = {'toolchain_release':release['tag_name'],
                'published':release['published_at'], 'inputs':results}
    (WORK/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'status':'ready','toolchain_release':release['tag_name'],
                      'files':[r['bytes'] for r in results]}),flush=True)


if __name__ == '__main__':
    if sys.argv[1:] == ['--verify-sdk-only']:
        print(json.dumps(verify_obs_source(), sort_keys=True))
    elif len(sys.argv) == 1:
        main()
    else:
        raise SystemExit('Usage: bootstrap_tools.py [--verify-sdk-only]')
