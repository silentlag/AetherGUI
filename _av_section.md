

<div align="center">

## Antivirus false positives

AetherGUI and AetherService are unsigned and do two things antivirus heuristics dislike: hook input and talk to HID devices. That is the entire job of a tablet driver, so generic ML detectors (Wacatac, Bearfoos, Wacapew, etc.) sometimes flag the binaries even when nothing malicious is happening.

**If your AV quarantines AetherGUI or AetherService:**

1. **Check the detection name.** Anything starting with `Wacatac`, `Bearfoos`, `Wacapew`, `Trojan:Script/...`, or labelled `Generic` / `Heuristic` / `ML.Detection` is almost certainly a false positive. Real malware is named specifically (for example `Trojan:Win32/Emotet!gen`).
2. **Verify the download.** Only trust binaries from the official GitHub Releases page, and compare the file's SHA-256 against what the release notes list.
3. **Add an exclusion.** Windows Security &rarr; Virus & threat protection &rarr; Manage settings &rarr; Exclusions &rarr; add the AetherGUI install folder.
4. **Submit it as a false positive.** Microsoft accepts submissions at <https://www.microsoft.com/en-us/wdsi/filesubmission>. Each report makes future releases less likely to be flagged.

**What the project already does to minimise this:**

- Both binaries ship a populated `VS_VERSION_INFO` block (CompanyName, ProductName, FileDescription, version numbers).
- Linker flags `/GUARD:CF` (Control Flow Guard) and `/CETCOMPAT` are enabled on both `AetherGUI.exe` and `AetherService.exe`. Many heuristics treat the absence of these as a suspicious signal.
- No packing, no obfuscation, no anti-debug tricks. The binaries are exactly what they look like: a regular Win32 application and a console helper.
- The source is open &mdash; anything an AV report claims the binary does can be verified directly in the tree.

Long-term the only real fix is signing the binaries with an Authenticode certificate. That is on the roadmap.

</div>
