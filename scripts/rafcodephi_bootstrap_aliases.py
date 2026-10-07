"""Verified, deterministic bootstrap aliases for RAFCODEPHI (stdlib only).

SYMLINKS.txt uses the upstream Termux "target←link" format. A link
is added only when its source-built executable exists in the archive.
This file changes archive packaging metadata, never device claims.
"""

REQUIRED_ALIASES = (
    ("bin/sh", "bash", "bin/bash"),
    ("libexec/termux-api", "termux-api-broadcast", "libexec/termux-api-broadcast"),
)


def normalize_bootstrap_aliases(archive_names, symlink_text):
    """Return (SYMLINKS.txt, link names); reject conflicts and absent targets."""
    links = {}
    for number, line in enumerate(symlink_text.splitlines(), 1):
        if not line:
            continue
        parts = line.split("←")
        if len(parts) != 2 or not all(parts):
            raise ValueError(f"malformed SYMLINKS.txt line {number}: {line!r}")
        target, link = parts
        if link.startswith("/") or ".." in link or "\\" in link:
            raise ValueError(f"unsafe symlink destination line {number}: {link!r}")
        if link in links and links[link] != target:
            raise ValueError(f"conflicting symlink destination line {number}: {link!r}")
        links[link] = target

    additions = []
    for link, expected_target, target_entry in REQUIRED_ALIASES:
        if link in archive_names:
            if link in links:
                raise ValueError(f"link conflicts with archive entry: {link}")
            continue
        if target_entry not in archive_names:
            raise ValueError(f"cannot create {link}; missing source-built target: {target_entry}")
        if link in links:
            if links[link] != expected_target:
                raise ValueError(f"unexpected target for {link}: {links[link]!r}")
        else:
            links[link] = expected_target
            additions.append(f"{expected_target}←{link}\n")

    if additions:
        if symlink_text and not symlink_text.endswith("\n"):
            symlink_text += "\n"
        symlink_text += "".join(additions)
    return symlink_text, set(links)
