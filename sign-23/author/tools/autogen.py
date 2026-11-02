"""autogen.py - patch a generated-constant region inside a hand-maintained file.

Generators patch constant blocks inside hand-written C headers and sources.
Each generator owns only the region delimited by:

    /* @@AUTOGEN:<tag>@@ BEGIN ... */
    ...generated content...
    /* @@AUTOGEN:<tag>@@ END */

The hand-written parts of the target file are preserved untouched, so
`make tables` regenerates the constants in place and `make check-consts` can
still regenerate-and-diff for reproducibility.

After the body is spliced in, the whole target file is run through clang-format
(the team's `format-c` style, `../.clang-format`, clang-format 14), so the
generated code AND the host file conform to the same style as the rest of the
tree.  The @@AUTOGEN markers are short single-line comments that survive
clang-format intact, so they remain matchable on the next regeneration.
"""
import os
import re
import shutil
import subprocess

_STYLE = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".clang-format"))
_CF_BIN = None


def _clang_format_bin():
    """Resolve a clang-format 14.x binary (the version the style targets, matching
    the format-c skill).  Cached.  Raises if none is available."""
    global _CF_BIN
    if _CF_BIN is not None:
        return _CF_BIN
    for cand in ("clang-format", "clang-format-14",
                 os.path.expanduser("~/.local/bin/clang-format")):
        path = shutil.which(cand)
        if not path and os.path.isfile(cand) and os.access(cand, os.X_OK):
            path = cand
        if not path:
            continue
        try:
            ver = subprocess.run([path, "--version"], capture_output=True,
                                 text=True).stdout
        except OSError:
            continue
        m = re.search(r"version (\d+)", ver)
        if m and m.group(1) == "14":
            _CF_BIN = path
            return _CF_BIN
    raise SystemExit(
        "autogen: clang-format 14.x not found (needed to format generated "
        "regions to the format-c style).  Install it like the format-c skill: "
        "`pip install --user clang-format==14.0.6`.")


def clang_format(text):
    """Format a C source string with the project's format-c style (stdin->stdout)."""
    cf = _clang_format_bin()
    r = subprocess.run([cf, "--style=file:" + _STYLE], input=text,
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("autogen: clang-format failed:\n" + r.stderr)
    return r.stdout


def clang_format_file(path):
    """Format `path` in place with the project's format-c style."""
    cf = _clang_format_bin()
    r = subprocess.run([cf, "--style=file:" + _STYLE, "-i", path],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("autogen: clang-format -i failed on %s:\n%s" % (path, r.stderr))


def patch_region(path, tag, body):
    """Replace the text between the @@AUTOGEN:<tag>@@ BEGIN/END markers in `path`,
    then clang-format the whole file (markers are short, single-line, and survive)."""
    with open(path) as f:
        text = f.read()
    pat = re.compile(
        r"(/\* @@AUTOGEN:" + re.escape(tag) + r"@@ BEGIN[^\n]*\*/\n)"
        r".*?"
        r"(^/\* @@AUTOGEN:" + re.escape(tag) + r"@@ END \*/)",
        re.DOTALL | re.MULTILINE)
    if not pat.search(text):
        raise SystemExit(
            f"autogen: marker @@AUTOGEN:{tag}@@ BEGIN/END not found in {path}")
    if not body.endswith("\n"):
        body += "\n"
    new = pat.sub(lambda m: m.group(1) + body + m.group(2), text)
    with open(path, "w") as f:
        f.write(new)
    clang_format_file(path)


def read_region(path, tag):
    """Return the text between the @@AUTOGEN:<tag>@@ BEGIN/END markers in `path`.
    Lets a reader (e.g. SigSize.py / check_rans.py) parse ONLY its own region when
    a file (irs_rans.h) hosts several per-mode `#if LITHIUM_MODE` blocks."""
    with open(path) as f:
        text = f.read()
    m = re.search(
        r"/\* @@AUTOGEN:" + re.escape(tag) + r"@@ BEGIN[^\n]*\*/\n"
        r"(.*?)"
        r"^/\* @@AUTOGEN:" + re.escape(tag) + r"@@ END \*/",
        text, re.DOTALL | re.MULTILINE)
    if not m:
        raise SystemExit(f"autogen: region @@AUTOGEN:{tag}@@ not found in {path}")
    return m.group(1)
