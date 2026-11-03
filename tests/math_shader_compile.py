"""CPU-only D3D HLSL compilation smoke; OBS effect parsing still needs libobs."""
import ctypes
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[1] / 'effects/beauty.effect').read_text(encoding='utf-8')
source = source[:source.index('technique Blur')]
source = re.sub(r'sampler_state textureSampler\s*\{[^}]+\};', 'SamplerState textureSampler;', source)
source = source.replace('uniform texture2d ', 'Texture2D ').replace(': TARGET', ': SV_Target')
payload = source.encode('utf-8')
dll = ctypes.WinDLL('D3DCompiler_47.dll')
dll.D3DCompile.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p,
                         ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p,
                         ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
dll.D3DCompile.restype = ctypes.c_long


def blob_bytes(blob):
    table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    get_pointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(table[3])
    get_size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(table[4])
    return ctypes.string_at(get_pointer(blob), get_size(blob))


def release(blob):
    if blob:
        table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(table[2])(blob)


for entry in ['PSBlur', 'PSComposite', 'PSMask']:
    code, errors = ctypes.c_void_p(), ctypes.c_void_p()
    result = dll.D3DCompile(payload, len(payload), b'beauty.effect', None, None,
                           entry.encode(), b'ps_5_0', 1 << 11, 0,
                           ctypes.byref(code), ctypes.byref(errors))
    try:
        if errors:
            print(blob_bytes(errors).decode('utf-8', errors='replace'))
        if result < 0:
            raise SystemExit(f'{entry} HLSL compile failed: {result}')
        print(f'PASS {entry} HLSL ps_5_0, {len(blob_bytes(code))} byte shader')
    finally:
        release(code)
        release(errors)
