"""Prepare reviewable notices and a source companion. Python stdlib only.

No extracted source is executed. collect is offline unless --download is given.
prepare additionally checks actual tool versions and packages git-archived app
sources. Neither command grants legal approval or publishes anything.
"""
import argparse
import hashlib
import json
import posixpath
import re
import shutil
import subprocess
import tarfile
import urllib.parse
import urllib.request
import uuid
import zipfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
MAX_TEXT = 8 * 1024 * 1024


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def safe_name(name):
    if (not name or "\\" in name or ":" in name or name.startswith("/")
            or ".." in PurePosixPath(name).parts):
        raise ValueError(f"Unsafe archive name: {name}")
    return str(PurePosixPath(name))


def load_manifest(path):
    manifest = json.loads(Path(path).read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or not manifest.get("sources"):
        raise ValueError("Unsupported or empty source manifest")
    names, ids = set(), set()
    for entry in manifest["sources"]:
        url = urllib.parse.urlparse(entry["url"])
        name = safe_name(posixpath.basename(url.path))
        if (url.scheme != "https" or not url.hostname or url.username or url.password
                or url.query or url.fragment or entry["group"] not in ("qt", "ifw")
                or not re.fullmatch(r"[a-f0-9]{64}", entry["sha256"])
                or not re.fullmatch(r"[a-z0-9.-]+", entry["id"])
                or name in names or entry["id"] in ids):
            raise ValueError("Invalid or duplicate source manifest entry")
        names.add(name)
        ids.add(entry["id"])
    return manifest


class HttpsRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        if urllib.parse.urlparse(newurl).scheme != "https":
            raise ValueError("Refusing a non-HTTPS source redirect")
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def obtain(entry, cache, download=False):
    target = Path(cache) / posixpath.basename(urllib.parse.urlparse(entry["url"]).path)
    if not target.exists():
        if not download:
            raise ValueError(f"Source missing from cache: {target.name}; use --download explicitly")
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_name(target.name + "." + uuid.uuid4().hex + ".partial")
        opener = urllib.request.build_opener(HttpsRedirects())
        # An interrupted download never becomes an accepted cache entry.
        try:
            with opener.open(entry["url"], timeout=120) as response, partial.open("xb") as out:
                for chunk in iter(lambda: response.read(1024 * 1024), b""):
                    out.write(chunk)
            if digest(partial) != entry["sha256"]:
                raise ValueError(f"Source SHA-256 mismatch: {target.name}")
            partial.replace(target)
        finally:
            partial.unlink(missing_ok=True)
    if not target.is_file() or digest(target) != entry["sha256"]:
        raise ValueError(f"Source SHA-256 mismatch: {target.name}")
    return target


def collect_notices(archive):
    """Keep original metadata, license bodies and copyright header blocks.

    This intentionally includes a source-level superset, not an assertion that
    every optional Qt component was linked into the delivered binaries.
    """
    sections = {}
    with tarfile.open(archive, "r:*") as tar:
        members = {}
        texts = {}
        headers = {}
        for item in tar:
            name = safe_name(item.name)
            if item.isfile():
                if name in members:
                    raise ValueError(f"Duplicate archive entry: {name}")
                members[name] = item
                base = posixpath.basename(name).lower()
                if (item.size <= MAX_TEXT and (base.endswith((".txt", ".md", ".rst"))
                        or base.startswith(("license", "licence", "copying", "copyright"))
                        or base in ("3rdparty", "authors", "notice", "qt_attribution.json", ".qt_attribution.json"))):
                    texts[name] = tar.extractfile(item).read()
                elif (("/src/3rdparty/" in name or "/src/libs/" in name
                       or len(PurePosixPath(name).parts) == 2)
                      and name.endswith((".c", ".cpp", ".h", ".hpp"))):
                    head = tar.extractfile(item).read(32768).decode("utf-8", errors="replace")
                    block = re.match(r"\s*(/\*.*?\*/|(?://[^\n]*\n)+)", head, re.S)
                    if block and re.search(r"copyright|license|public domain", block[0], re.I):
                        headers[name + " [header notice]"] = block[0].strip()

        def text(name):
            item = members.get(name)
            if item is None or item.size > MAX_TEXT:
                raise ValueError(f"Missing or oversized notice: {name}")
            data = texts[name] if name in texts else tar.extractfile(item).read()
            try:
                return data.decode("utf-8-sig")
            except UnicodeDecodeError:
                return data.decode("latin-1")

        for name in sorted(members):
            base = posixpath.basename(name).lower()
            legal = ("/LICENSES/" in name or base.startswith(("license", "licence", "copying", "copyright"))
                     or base in ("3rdparty", "authors", "notice", "notice.txt"))
            if legal:
                sections[name] = text(name)
            if (base in ("qt_attribution.json", ".qt_attribution.json")
                    and not set(PurePosixPath(name).parts) & {"tests", "examples"}):
                content = text(name)
                sections[name] = content
                # Some upstream attribution strings contain literal tab characters.
                entries = json.loads(content, strict=False)
                if isinstance(entries, dict):
                    entries = [entries]
                for entry in entries:
                    files = entry.get("LicenseFile", [])
                    if isinstance(files, str):
                        files = [files]
                    for file in files:
                        path = safe_name(posixpath.normpath(posixpath.join(posixpath.dirname(name), file)))
                        if path.split("/")[0] != name.split("/")[0]:
                            raise ValueError("License reference escapes source root")
                        sections[path] = text(path)
        sections.update(headers)
        if not sections:
            raise ValueError(f"No license material found in {Path(archive).name}")
    return "\n\n".join(f"===== {name} =====\n{value}" for name, value in sorted(sections.items())) + "\n"


def write_text(path, text):
    with Path(path).open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(text)


def collect(manifest, cache, output, download=False):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    # Never leave a source-offer file from a different run in a review directory.
    if any(output.iterdir()):
        raise ValueError("Use an empty output directory; existing material is not overwritten")
    groups = {"qt": [], "ifw": []}
    archives = []
    for entry in manifest["sources"]:
        archive = obtain(entry, cache, download)
        archives.append(archive)
        groups[entry["group"]].append(f"SOURCE: {entry['id']}\nURL: {entry['url']}\nSHA-256: {entry['sha256']}\n\n"
                                      + collect_notices(archive))
    heading = ("Original upstream license and attribution material.\n"
               "This is a conservative source-level inventory, including optional/unused code.\n"
               "It does not assert that every listed component is present in the binary.\n"
               "Review the actual build configuration and any vendor patches before publication.\n\n")
    for group, filename in (("qt", "QT-NOTICES.txt"), ("ifw", "IFW-NOTICES.txt")):
        write_text(output / filename, heading + "\n".join(groups[group]))
    write_text(output / "APPLICATION-LICENSE.txt", (ROOT / "LICENSE").read_text(encoding="utf-8"))
    write_text(output / "SOURCE-MANIFEST.json", json.dumps(manifest, indent=2) + "\n")
    return archives


def check_versions(manifest, qt_version, ifw_output):
    match = re.search(r"IFW Version: ([\d.]+), built with Qt ([\d.]+)\.", ifw_output)
    if (qt_version.strip() != manifest["qtVersion"] or not match
            or match.groups() != (manifest["ifwVersion"], manifest["ifwQtVersion"])
            or manifest["ifwCommit"] not in ifw_output
            or manifest["ifwArchiveVersions"] not in ifw_output):
        raise ValueError("Actual Qt/IFW binaries do not match the reviewed source profile")


def prepare(manifest, cache, output, bundle_directory, version, repository, commit,
            qt_path, ifw_path, application_source, download=False, supplements=None):
    if (not re.fullmatch(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", version)
            or not re.fullmatch(r"[\w.-]+/[\w.-]+", repository)
            or not re.fullmatch(r"[a-f0-9]{40}", commit)):
        raise ValueError("Expected a stable version, GitHub owner/repository and immutable commit")
    qt = Path(qt_path) / "bin/qmake.exe"
    ifw = Path(ifw_path) / "bin/installerbase.exe"
    qt_version = subprocess.check_output([str(qt), "-query", "QT_VERSION"], text=True)
    ifw_info = subprocess.check_output([str(ifw), "--version"], text=True, stderr=subprocess.STDOUT)
    check_versions(manifest, qt_version, ifw_info)
    application_source = Path(application_source)
    with zipfile.ZipFile(application_source) as source:
        if source.comment != commit.encode("ascii"):
            raise ValueError("Application source must be git archive ZIP for the release commit")
        for name in source.namelist():
            safe_name(name)
        if source.read("LICENSE").decode("utf-8").replace("\r\n", "\n") != (ROOT / "LICENSE").read_text(encoding="utf-8"):
            raise ValueError("Application source archive has a different license")
        if source.testzip():
            raise ValueError("Application source ZIP failed validation")
    archives = collect(manifest, cache, output, download)
    output = Path(output)
    supplied = {}
    if supplements:
        reserved = {"APPLICATION-LICENSE.txt", "QT-NOTICES.txt", "IFW-NOTICES.txt",
                    "SOURCE-MANIFEST.json", "SOURCE-OFFER.txt", "BUILD-INFO.json"}
        for file in sorted(Path(supplements).iterdir()):
            if (not file.is_file() or file.is_symlink() or file.name in reserved
                    or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", file.name)):
                raise ValueError("Supplement files must be regular, flat and must not replace generated notices")
            shutil.copyfile(file, output / file.name)
            supplied[file.name] = digest(output / file.name)
    bundle_directory = Path(bundle_directory)
    bundle_directory.mkdir(parents=True, exist_ok=True)
    bundle_name = f"AgentNodify-{version}-Sources.zip"
    bundle = bundle_directory / bundle_name
    record = bundle_directory / "distribution-release.json"
    if bundle.exists() or record.exists():
        raise ValueError("Source bundle or release record already exists; use a fresh directory")
    info = {
        "schema": 1, "version": version, "repository": repository, "commit": commit,
        "profile": manifest["profile"], "applicationSourceSha256": digest(application_source),
        "qtVersion": qt_version.strip(), "ifwVersionOutput": ifw_info.strip(),
        "qmakeSha256": digest(qt), "installerbaseSha256": digest(ifw),
        "supplements": supplied,
        "approval": "Requires distributor review; preparation is not legal approval"
    }
    write_text(output / "BUILD-INFO.json", json.dumps(info, indent=2) + "\n")
    link = f"https://github.com/{repository}/releases/download/v{version}/{bundle_name}"
    write_text(output / "SOURCE-OFFER.txt", f"""Source access for AgentNodify {version}

This distribution is accompanied by {bundle_name} on the same release page:
https://github.com/{repository}/releases/tag/v{version}
Direct source-companion download: {link}

The publisher uploads this source companion before publishing the binary release.
The companion contains the unmodified upstream source archives listed in
SOURCE-MANIFEST.json, their SHA-256 hashes, the application source at commit
{commit}, and these distribution notices/build records.
No additional fee is charged for access to these source materials.

Application build: follow spec/development.md in application-source.zip,
using -DAGENTNODIFY_VERSION={version}. Installer configuration and scripts are in
cmake/, packaging/ and scripts/ in that source archive. Upstream Qt configure
scripts, per-module CMake files and IFW INSTALL/coin instructions are retained
inside the original source archives. See BUILD-INFO.json for the actual tool
versions and binary fingerprints. No local private paths or credentials are
included in that record.

The upstream archives are unmodified by this preparation process. This does not
assert that upstream prebuilt binaries contain no vendor patches. The distributor
must review exact binary/source correspondence and supply any required vendor
patches, configuration and build information before approving publication.
The generic source inventory is a superset, not a binary linkage report.
Additional distributor-supplied patches/build information, if any, are listed
under supplements in BUILD-INFO.json and included in this source companion.
Microsoft runtime/compiler source is not included or relicensed here.
""")
    notice_hashes = {}
    with zipfile.ZipFile(bundle, "x", compression=zipfile.ZIP_STORED) as archive:
        archive.write(application_source, "application-source.zip")
        for source in archives:
            archive.write(source, "upstream/" + source.name)
        for file in sorted(output.iterdir()):
            if file.is_file():
                data = file.read_bytes()
                archive.writestr("distribution/" + file.name, data)
                notice_hashes[file.name] = hashlib.sha256(data).hexdigest()
    info["sourceBundle"] = bundle_name
    info["sourceBundleSha256"] = digest(bundle)
    info["noticeHashes"] = notice_hashes
    write_text(record, json.dumps(info, indent=2) + "\n")
    return bundle


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("collect", "prepare"))
    parser.add_argument("--manifest", type=Path, default=ROOT / "packaging/sources.json")
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--download", action="store_true", help="explicitly allow pinned HTTPS downloads")
    parser.add_argument("--supplement-directory", type=Path, help="reviewed vendor patches/build information (flat files)")
    for option in ("bundle-directory", "version", "repository", "commit", "qt-path", "ifw-path", "application-source"):
        parser.add_argument("--" + option)
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.command == "collect":
        collect(manifest, args.cache, args.output, args.download)
        print(f"Review material: {args.output} (no source offer or approval created)")
    else:
        values = (args.bundle_directory, args.version, args.repository, args.commit,
                  args.qt_path, args.ifw_path, args.application_source)
        if not all(values):
            parser.error("prepare requires all build, repository and bundle options")
        print(prepare(manifest, args.cache, args.output, *values, download=args.download,
                      supplements=args.supplement_directory))


if __name__ == "__main__":
    main()
