"""Prepare an import library from the installed OBS ABI, without changing OBS."""
import argparse
import hashlib
import json
import pathlib
import subprocess

import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
SDK = ROOT / "third_party/obs-sdk"
TOOLCHAIN = ROOT / "third_party/toolchain/llvm-mingw-20261020-ucrt-x86_64"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--obs-root", type=pathlib.Path, default=pathlib.Path("C:/Program Files/obs-studio"))
    args = parser.parse_args()
    obs_dll = args.obs_root.resolve() / "bin/64bit/obs.dll"
    for required in (obs_dll, TOOLCHAIN / "bin/llvm-dlltool.exe"):
        if not required.is_file():
            raise RuntimeError(f"Missing build input: {required}")
    pe = pefile.PE(str(obs_dll), fast_load=False)
    if pe.FILE_HEADER.Machine != 0x8664:
        raise RuntimeError("Expected an x64 OBS DLL")
    symbols = sorted(x.name.decode("ascii") for x in pe.DIRECTORY_ENTRY_EXPORT.symbols if x.name)
    libdir = SDK / "lib"
    generated = SDK / "generated"
    libdir.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)
    definition = libdir / "obs.def"
    definition.write_text("LIBRARY obs.dll\nEXPORTS\n" + "\n".join(symbols) + "\n", encoding="ascii")
    subprocess.run([str(TOOLCHAIN / "bin/llvm-dlltool.exe"), "-m", "i386:x86-64", "-D", "obs.dll", "-d", str(definition), "-l", str(libdir / "libobs.dll.a")], check=True)
    (generated / "obsconfig.h").write_text(
        "// Generated for the OBS 33.0.1 public headers. Not an OBS installation configuration.\n"
        "#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n", encoding="ascii")
    evidence = {
        "dll": str(obs_dll), "sha256": hashlib.sha256(obs_dll.read_bytes()).hexdigest(),
        "machine": "x86_64", "exports": len(symbols),
        "source_headers": "https://github.com/obsproject/obs-studio/tree/33.0.1/libobs",
        "import_library": str(libdir / "libobs.dll.a"),
    }
    proof = ROOT / "work/build-tools/obs-sdk-proof.json"
    proof.parent.mkdir(parents=True, exist_ok=True)
    proof.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print(json.dumps(evidence))

if __name__ == "__main__":
    main()
