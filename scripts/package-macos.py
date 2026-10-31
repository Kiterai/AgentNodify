#!/usr/bin/env python3
"""Build a local-validation macOS PKG. No sudo, credentials or publication."""
import argparse
import hashlib
import json
import os
import platform
import plistlib
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IDENTIFIER = "org.agentnodify.AgentNodify"
MACHO = {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
         b"\xfe\xed\xfa\xce", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca"}


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def macho_files(app):
    for path in sorted(app.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            if stream.read(4) in MACHO:
                yield path


def dependencies(path):
    output = run("/usr/bin/otool", "-L", path, capture_output=True, text=True).stdout
    ids = run("/usr/bin/otool", "-D", path, capture_output=True, text=True).stdout.splitlines()[1:]
    return [line.strip().split(" (", 1)[0] for line in output.splitlines()[1:]
            if line.startswith("\t") and line.strip().split(" (", 1)[0] not in ids]


def is_system(path):
    return path.startswith(("/usr/lib/", "/System/Library/"))


def inside_bundle(path, app):
    try:
        path.resolve().relative_to(app.resolve())
        return True
    except ValueError:
        return False


def repair_and_verify_dependencies(app):
    """Only edit the copied bundle, never the Qt kit or Homebrew libraries.

    macdeployqt 6.12 can miss a transitive @rpath reference (Homebrew Brotli).
    Resolve it only to an already bundled library, otherwise fail closed.
    """
    frameworks = app / "Contents/Frameworks"
    for binary in macho_files(app):
        for dep in dependencies(binary):
            if is_system(dep):
                continue
            if dep.startswith("@rpath/"):
                target = frameworks / dep[len("@rpath/"):]
                if not target.is_file() or not inside_bundle(target, app):
                    raise ValueError(f"Unresolved bundled dependency: {binary}: {dep}")
                relative = os.path.relpath(target, binary.parent)
                run("/usr/bin/install_name_tool", "-change", dep, "@loader_path/" + relative, binary)
            elif dep.startswith("@loader_path/"):
                target = binary.parent / dep[len("@loader_path/"):]
                if not target.is_file() or not inside_bundle(target, app):
                    raise ValueError(f"Missing loader-relative dependency: {binary}: {dep}")
            elif dep.startswith("@executable_path/"):
                target = app / "Contents/MacOS" / dep[len("@executable_path/"):]
                if not target.is_file() or not inside_bundle(target, app):
                    raise ValueError(f"Missing executable-relative dependency: {binary}: {dep}")
            else:
                raise ValueError(f"External dependency in package: {binary}: {dep}")


def package(build, qt, version, reuse_build=False, legal_directory=None):
    if platform.system() != "Darwin":
        raise ValueError("macOS is required")
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("Version must be a stable major.minor.patch")
    build, qt = build.resolve(), qt.resolve()
    deploy = qt / "bin/macdeployqt"
    if not deploy.is_file():
        raise ValueError("Qt kit must include bin/macdeployqt")
    build.mkdir(parents=True, exist_ok=True)
    legal_args = []
    if legal_directory:
        legal_directory = Path(legal_directory).resolve(strict=True)
        legal_args = [f"-DAGENTNODIFY_LEGAL_DIRECTORY={legal_directory}"]
    if not reuse_build:
        run("cmake", "-S", ROOT, "-B", build, "-G", "Unix Makefiles",
            "-DCMAKE_BUILD_TYPE=Release", f"-DQt6_ROOT={qt}", f"-DAGENTNODIFY_VERSION={version}", *legal_args)
        run("cmake", "--build", build, "--parallel")
    cache = (build / "CMakeCache.txt").read_text()
    if f"AGENTNODIFY_VERSION:STRING={version}\n" not in cache or "CMAKE_BUILD_TYPE:STRING=Release\n" not in cache:
        raise ValueError("Package requires a matching-version Release build")
    tmp = build / "test-tmp"
    tmp.mkdir(exist_ok=True)
    run("ctest", "--test-dir", build, "--output-on-failure", env={**os.environ, "TMPDIR": str(tmp)})
    # A fresh stage prevents a previous deploy from hiding missing files.
    stage = Path(tempfile.mkdtemp(prefix="macos-package-", dir=build))
    payload = stage / "payload"
    payload.mkdir()
    app = payload / "AgentNodify.app"
    shutil.copytree(build / "AgentNodify.app", app, symlinks=True,
                    ignore=shutil.ignore_patterns("smoke-instance"))
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    if info.get("CFBundleShortVersionString") != version or info.get("CFBundleIdentifier") != IDENTIFIER:
        raise ValueError("Application bundle identity/version does not match package")
    resources = app / "Contents/Resources"
    resources.mkdir(exist_ok=True)
    shutil.copy2(ROOT / "LICENSE", resources / "LICENSE")
    shutil.copytree(ROOT / "licenses", resources / "licenses")
    if legal_directory:
        shutil.copytree(legal_directory, resources / "licenses/distribution")
    shutil.copy2(ROOT / "packaging/THIRD-PARTY-NOTICES.txt", resources / "licenses/THIRD-PARTY-NOTICES.txt")
    shutil.copytree(ROOT / "examples", resources / "examples", ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    shutil.copy2(ROOT / "packaging/macos/INSTALL-NOTES.txt", resources / "INSTALL-NOTES.txt")
    # Do not sign until deployment and relocation edits are complete.
    result = run(deploy, app, "-no-codesign", capture_output=True, text=True)
    (stage / "macdeployqt.log").write_text(result.stdout + result.stderr)
    errors = [line for line in (result.stdout + result.stderr).splitlines() if "ERROR:" in line]
    # Permit only the known missed-rpath diagnostic; the verifier below must
    # still resolve every load reference to a bundled or system library.
    if any(not ("Cannot resolve rpath" in line or "using QList" in line) for line in errors):
        raise ValueError("macdeployqt reported errors; inspect " + str(stage / "macdeployqt.log"))
    repair_and_verify_dependencies(app)
    run("/usr/bin/codesign", "--force", "--deep", "--sign", "-", "--timestamp=none", app)
    run("/usr/bin/codesign", "--verify", "--deep", "--strict", app)
    components = stage / "components.plist"
    # Explicitly treat the app as one component, not its nested Qt frameworks.
    entries = [{"RootRelativeBundlePath": "AgentNodify.app", "BundleIsRelocatable": False,
                "BundleHasStrictIdentifier": True, "BundleIsVersionChecked": True,
                "BundleOverwriteAction": "upgrade"}]
    components.write_bytes(plistlib.dumps(entries))
    output = build / "installer"
    output.mkdir(exist_ok=True)
    arch = platform.machine()
    installer = output / f"AgentNodify-{version}-macOS-{arch}-Setup.pkg"
    if installer.exists():
        raise FileExistsError(f"Refusing to overwrite installer: {installer}")
    run("/usr/bin/pkgbuild", "--root", payload, "--component-plist", components,
        "--identifier", IDENTIFIER, "--version", version, "--install-location", "/Applications", installer)
    checksum = hashlib.sha256(installer.read_bytes()).hexdigest()
    (output / (installer.name + ".sha256")).write_text(f"{checksum}  {installer.name}\n")
    report = {"schema": 1, "version": version, "architecture": arch, "installer": installer.name,
              "sha256": checksum, "applicationSignature": "ad-hoc", "installerSigned": False,
              "notarized": False, "publicationApproved": False, "stage": str(stage)}
    (output / "package-validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Ready (local validation only): {installer}")
    return installer


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, default=ROOT / "build-macos-package")
    parser.add_argument("--qt-path", type=Path, default=os.environ.get("Qt6_ROOT"))
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--legal-directory", type=Path)
    parser.add_argument("--reuse-build", action="store_true", help="Require an existing matching Release build; still run CTest")
    args = parser.parse_args()
    if args.qt_path is None:
        parser.error("Specify --qt-path or Qt6_ROOT")
    package(args.build_directory, args.qt_path, args.version, args.reuse_build, args.legal_directory)
