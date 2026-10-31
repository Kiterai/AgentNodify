#!/usr/bin/env python3
"""Inspect a PKG offline; real install/uninstall is opt-in on a GitHub macOS runner."""
import argparse
import hashlib
import json
import os
import platform
import plistlib
import subprocess
import tempfile
from pathlib import Path


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def inspect(installer, directory):
    expanded = directory / "expanded"
    run("/usr/sbin/pkgutil", "--expand-full", installer, expanded)
    apps = list(expanded.glob("**/Payload/AgentNodify.app"))
    if len(apps) != 1:
        raise ValueError("Expected exactly one AgentNodify application payload")
    app = apps[0]
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    if info.get("CFBundleIdentifier") != "org.agentnodify.AgentNodify":
        raise ValueError("Unexpected application identity")
    for relative in ["Contents/MacOS/AgentNodify", "Contents/Resources/LICENSE",
                     "Contents/Resources/INSTALL-NOTES.txt", "Contents/Resources/licenses/THIRD-PARTY-NOTICES.txt",
                     "Contents/Resources/examples/script.flow.json", "Contents/Frameworks/QtCore.framework/QtCore",
                     "Contents/PlugIns/platforms/libqcocoa.dylib"]:
        if not (app / relative).is_file():
            raise ValueError("Missing payload file: " + relative)
    for path in app.rglob("*"):
        if path.name in {"workspace.json", "instance.lock", "smoke-instance", "CMakeCache.txt"} or path.name.startswith(("fake_agent", "fake_catalog", "agentnodify_")):
            raise ValueError("Development/user data in installer: " + str(path))
    run("/usr/bin/codesign", "--verify", "--deep", "--strict", app)
    return info["CFBundleShortVersionString"]


def roundtrip(installer, version):
    if os.environ.get("GITHUB_ACTIONS") != "true" or os.environ.get("RUNNER_OS") != "macOS":
        raise ValueError("Real installation is restricted to disposable GitHub macOS runners")
    app = Path("/Applications/AgentNodify.app")
    identifier = "org.agentnodify.AgentNodify"
    if app.exists() or app.is_symlink():
        raise ValueError("Refusing to overwrite an existing application")
    receipt = subprocess.run(["/usr/sbin/pkgutil", "--pkg-info", identifier], capture_output=True)
    if receipt.returncode == 0:
        raise ValueError("Refusing to overwrite an existing package receipt")
    try:
        run("sudo", "-n", "/usr/sbin/installer", "-pkg", installer, "-target", "/")
        record = plistlib.loads(run("/usr/sbin/pkgutil", "--pkg-info-plist", identifier, capture_output=True).stdout)
        if record["pkg-version"] != version:
            raise ValueError("Installed receipt version mismatch")
        run("/usr/bin/codesign", "--verify", "--deep", "--strict", app)
        # No developer Qt/plugin/library paths; smoke mode never invokes agent CLIs.
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("QT_", "DYLD_"))}
        env.update(PATH="/usr/bin:/bin:/usr/sbin:/sbin", QT_QPA_PLATFORM="cocoa")
        run(app / "Contents/MacOS/AgentNodify", "--smoke-test", env=env, timeout=30)
        run("/usr/bin/codesign", "--verify", "--deep", "--strict", app)
    finally:
        # Only this invocation's application/receipt, and never user data.
        if app.is_symlink():
            raise ValueError("Refusing cleanup of a replaced application symlink")
        if app.exists():
            run("sudo", "-n", "/bin/rm", "-r", str(app))
        run("sudo", "-n", "/usr/sbin/pkgutil", "--forget", identifier)
    if app.exists():
        raise ValueError("Application cleanup failed")
    if subprocess.run(["/usr/sbin/pkgutil", "--pkg-info", identifier], capture_output=True).returncode == 0:
        raise ValueError("Package receipt cleanup failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installer", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    installer = args.installer.resolve(strict=True)
    report = {"schema": 1, "installer": installer.name, "sha256": hashlib.sha256(installer.read_bytes()).hexdigest(),
              "success": False, "installed": False, "notarized": False,
              "commit": os.environ.get("GITHUB_SHA"), "architecture": platform.machine(), "checks": []}
    try:
        with tempfile.TemporaryDirectory(prefix="agentnodify-pkg-") as temporary:
            report["version"] = inspect(installer, Path(temporary))
            report["checks"].append("payload")
        if args.install:
            roundtrip(installer, report["version"])
            report["installed"] = True
            report["checks"].extend(["receipt", "cocoa", "signature", "uninstall", "receipt_removed"])
        report["success"] = True
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
