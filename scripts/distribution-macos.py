"""macOS official-Qt source companion. Never grants publication approval."""
import argparse
import json
import re
import subprocess
import zipfile
from pathlib import Path

import distribution as common


def prepare(manifest, cache, output, bundle_directory, version, repository, commit,
            qt_path, application_source, architecture, download=False):
    if (not re.fullmatch(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", version)
            or not re.fullmatch(r"[\w.-]+/[\w.-]+", repository)
            or not re.fullmatch(r"[a-f0-9]{40}", commit)
            or architecture not in ("arm64", "x86_64")
            or manifest.get("profile") != "macos-official-qt-6.12.0"
            or any(entry["group"] != "qt" for entry in manifest["sources"])):
        raise ValueError("Expected the official macOS Qt profile and immutable release identity")
    qt = Path(qt_path) / "bin/qmake"
    qt_version = subprocess.check_output([str(qt), "-query", "QT_VERSION"], text=True).strip()
    if qt_version != manifest["qtVersion"]:
        raise ValueError("Actual Qt does not match the macOS source profile")
    application_source = Path(application_source)
    with zipfile.ZipFile(application_source) as source:
        if source.comment != commit.encode("ascii") or source.testzip():
            raise ValueError("Application source must be git archive ZIP for the release commit")
        for name in source.namelist():
            common.safe_name(name)
        if source.read("LICENSE").decode("utf-8").replace("\r\n", "\n") != (common.ROOT / "LICENSE").read_text(encoding="utf-8"):
            raise ValueError("Application source license mismatch")
    output, bundle_directory = Path(output), Path(bundle_directory)
    archives = common.collect(manifest, cache, output, download)
    # This native PKG contains no IFW or Maintenance Tool.
    (output / "IFW-NOTICES.txt").unlink()
    bundle_directory.mkdir(parents=True, exist_ok=True)
    bundle_name = f"AgentNodify-{version}-macOS-{architecture}-Sources.zip"
    bundle = bundle_directory / bundle_name
    record = bundle_directory / f"macos-{architecture}-distribution-release.json"
    if bundle.exists() or record.exists():
        raise ValueError("Source companion already exists; use a fresh directory")
    info = {"schema": 1, "version": version, "repository": repository, "commit": commit,
            "architecture": architecture, "profile": manifest["profile"], "qtVersion": qt_version,
            "applicationSourceSha256": common.digest(application_source), "qmakeSha256": common.digest(qt),
            "approval": "Requires independent macOS distributor review; preparation is not approval"}
    common.write_text(output / "BUILD-INFO.json", json.dumps(info, indent=2) + "\n")
    common.write_text(output / "SOURCE-OFFER.txt", f"""Corresponding sources for AgentNodify {version}, macOS {architecture}

Source companion: {bundle_name}
https://github.com/{repository}/releases/download/v{version}/{bundle_name}
Release: https://github.com/{repository}/releases/tag/v{version}
Application commit: {commit}

This companion is uploaded before the release becomes public and contains the
matching application git archive, pinned original Qt source archives, original
licenses/attributions, source manifest and build records. No extra access fee.
Build the application with scripts/package-macos.py --version {version} using
the official Qt {qt_version} macOS kit. See spec/macos-packaging.md and the Qt
source configure/build files. This inventory is a conservative superset and
does not assert every optional Qt component is linked. The native Apple PKG
does not contain IFW. Apple system frameworks and developer tools are not
redistributed as part of this companion.

The distributor must review the official binary/source correspondence and
supply any necessary vendor patches/configuration/build information before
approving publication. The Windows and Homebrew profiles do not cover this
official macOS kit. Preserve source access alongside the binary release.
""")
    hashes = {}
    with zipfile.ZipFile(bundle, "x", compression=zipfile.ZIP_STORED) as archive:
        archive.write(application_source, "application-source.zip")
        for path in archives:
            archive.write(path, "upstream/" + path.name)
        for path in sorted(output.iterdir()):
            if not path.is_file() or path.is_symlink():
                raise ValueError("Unexpected distribution material")
            archive.write(path, "distribution/" + path.name)
            hashes[path.name] = common.digest(path)
    info.update(sourceBundle=bundle_name, sourceBundleSha256=common.digest(bundle), noticeHashes=hashes)
    common.write_text(record, json.dumps(info, indent=2) + "\n")
    return bundle


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=common.ROOT / "packaging/macos/sources.json")
    parser.add_argument("--download", action="store_true")
    for option in ("cache", "output", "bundle-directory", "version", "repository", "commit", "qt-path", "application-source", "architecture"):
        parser.add_argument("--" + option, required=True)
    args = vars(parser.parse_args())
    manifest = common.load_manifest(args.pop("manifest"))
    print(prepare(manifest, **args))
