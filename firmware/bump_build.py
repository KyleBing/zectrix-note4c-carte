# PlatformIO pre-script: every `pio run` bumps APP_BUILD in include/app_version.h
Import("env")  # noqa: F821 — provided by PlatformIO

from pathlib import Path
import re

header = Path(env["PROJECT_DIR"]) / "include" / "app_version.h"  # noqa: F821
text = header.read_text(encoding="utf-8")
m = re.search(r"static constexpr int APP_BUILD = (\d+);", text)
if not m:
    raise SystemExit("[bump_build] APP_BUILD not found in app_version.h")
n = int(m.group(1)) + 1
text2, count = re.subn(
    r"static constexpr int APP_BUILD = \d+;",
    f"static constexpr int APP_BUILD = {n};",
    text,
    count=1,
)
if count != 1:
    raise SystemExit("[bump_build] failed to rewrite APP_BUILD")
header.write_text(text2, encoding="utf-8")
print(f"[bump_build] APP_BUILD {m.group(1)} -> {n}")
