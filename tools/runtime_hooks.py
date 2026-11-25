"""Generate the interpreter's bank guards from the same hook list as NESRecomp."""
import json
from pathlib import Path
import sys
import tomllib
import zlib


def write_hook_table(config, destination):
    with Path(config).open("rb") as source:
        hooks = tomllib.load(source)["mod_function_hook"]
    lines = ["const CycHookSite cyc_native_hook_sites[] = {"]
    for hook in hooks:
        signature = bytes.fromhex(hook["bytes"])
        if not 1 <= len(signature) <= 16:
            raise ValueError(f"Firma de banco no válida: {hook['id']}")
        lines.append(f"    {{ {json.dumps(hook['id'])}, 0x{hook['addr']:04x}, "
                     f"{len(signature)}, 0x{zlib.crc32(signature):08x}u }},")
    lines.extend(["};", f"const uint32_t cyc_native_hook_site_count = {len(hooks)}u;"])
    Path(destination).write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    write_hook_table(*sys.argv[1:])
