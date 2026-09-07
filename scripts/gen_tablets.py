#!/usr/bin/env python3
"""Generate the OpenTabletDriver device section for EmbeddedConfig.h.

Usage:
    python scripts/gen_tablets.py <path-to-OpenTabletDriver-repo>

Reads every tablet JSON in the repo's configuration directory, converts
identifiers that are missing from the hand-written database into our
config language, and rewrites the configDataOtdMissing section.

The OTD tablet configs are LGPLv3 data; they are only read at conversion
time and kept as a generated, clearly-marked section.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(ROOT, 'AetherService', 'EmbeddedConfig.h')

SECTION_RE = re.compile(
    r'(const char\* configDataOtdMissing = R"CFG\(\r?\n)(.*?)(^\)CFG";)',
    re.S | re.M)


def iter_tablet_files(otd_root):
    cfg_dir = None
    for cand in ('OpenTabletDriver.Configurations', 'tablets/OpenTabletDriver'):
        p = os.path.join(otd_root, cand)
        if os.path.isdir(p):
            cfg_dir = p
            break
    if cfg_dir is None:
        sys.exit('No configuration folder found under %r '
                 '(looked for OpenTabletDriver.Configurations)' % otd_root)
    for base, _dirs, files in os.walk(cfg_dir):
        for fn in files:
            if fn.endswith('.json'):
                yield os.path.join(base, fn)


def existing_vid_pids(header_text):
    pairs = set()
    for m in re.finditer(r'^Tablet\s+(0x[0-9a-fA-F]{4})\s+(0x[0-9a-fA-F]{4})',
                         header_text, re.M):
        pairs.add((int(m.group(1), 16), int(m.group(2), 16)))
    return pairs


def ascii_hex(s):
    return ' '.join('0x%02X' % b for b in s.encode('utf-8'))


def block_for(path, dev):
    name = dev.get('Name') or os.path.splitext(os.path.basename(path))[0]
    idents = dev.get('DigitizerIdentifiers') or dev.get('DeviceIdentifiers') or []
    # newer OTD splits digitizer/aux with a "DeviceKind"/"Digitizer" hint; keep
    # entries that look like digitizers or have no hint at all
    picked = []
    for ident in idents:
        kind = str(ident.get('DeviceKind', ident.get('DeviceType', ''))).lower()
        if 'aux' in kind or 'mouse' in kind or 'auxiliary' in kind:
            continue
        picked.append(ident)
    if not picked:
        return None
    lines = []
    emitted_any = False
    for ident in picked:
        vid, pid = ident.get('Vid'), ident.get('Pid')
        if not vid or not pid:
            continue
        length = int(ident.get('InputReportLength') or 0)
        iface = ident.get('Interface')
        strings = ident.get('DeviceStrings') or {}
        string_args = ''
        for idx, (sid, match) in enumerate(strings.items()):
            if idx >= 2:
                break
            string_args += ' %s "%s"' % (sid, match)
        if len(strings) == 1:
            string_args += ' 0 ""'  # keep positional order: sid1 match1 sid2 match2 iface
        iface_arg = ' %d' % int(iface) if iface is not None else ' -1'
        lines.append('# %s (from %s)' % (name, os.path.basename(path)))
        lines.append('Tablet 0x%04X 0x%04X 0 0 %d%s%s' % (
            vid & 0xFFFF, pid & 0xFFFF, length, string_args, iface_arg))
        lines.append('Name "%s"' % name)
        if length > 0:
            lines.append('ReportLength %d' % length)
        lines.append('Type HIDDigitizer')
        inits = ident.get('InitializationStrings') or dev.get('InitializationStrings') or []
        for init in inits:
            if init:
                lines.append('InitReport %s' % ascii_hex(init))
        lines.append('')
        emitted_any = True
    return '\n'.join(lines) + '\n' if emitted_any else None


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    otd_root = sys.argv[1]
    header = open(TARGET, 'r', encoding='utf-8', newline='').read()

    have = existing_vid_pids(header)
    new_blocks = []
    seen = set(have)
    files = sorted(iter_tablet_files(otd_root))
    for path in files:
        try:
            dev = json.load(open(path, 'r', encoding='utf-8-sig'))
        except (ValueError, OSError):
            continue
        if not isinstance(dev, dict):
            continue
        for ident in dev.get('DigitizerIdentifiers') or dev.get('DeviceIdentifiers') or []:
            vid, pid = ident.get('Vid'), ident.get('Pid')
            if vid and pid and (vid & 0xFFFF, pid & 0xFFFF) in seen:
                break  # already covered, skip whole device
        else:
            key = None
            for ident in dev.get('DigitizerIdentifiers') or dev.get('DeviceIdentifiers') or []:
                vid, pid = ident.get('Vid'), ident.get('Pid')
                if vid and pid:
                    key = (vid & 0xFFFF, pid & 0xFFFF)
                    break
            if key is None:
                continue
            block = block_for(path, dev)
            if block:
                new_blocks.append(block)
                seen.add(key)

    body = ('# === GENERATED from OpenTabletDriver configs (LGPLv3 data), '
            'do not hand-edit. ===\n'
            '# Regenerate: python scripts/gen_tablets.py <otd-repo>\n\n'
            + '\n'.join(new_blocks)).replace('\r\n', '\n')

    if not SECTION_RE.search(header):
        sys.exit('configDataOtdMissing section not found in EmbeddedConfig.h')

    nl = '\r\n' if '\r\nconfigDataOtdMissing' in header or header.count('\r\n') > 100 else '\n'
    body = body.replace('\n', nl)
    new_header = SECTION_RE.sub(lambda m: m.group(1) + body + nl + m.group(3), header, count=1)
    open(TARGET, 'w', encoding='utf-8', newline='').write(new_header)
    print('devices added: %d (embedded already had %d)' % (len(new_blocks), len(have)))


if __name__ == '__main__':
    main()
