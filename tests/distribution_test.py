import hashlib
import importlib.util
import io
import json
import shutil
import sys
import tarfile
import unittest
import zipfile
import uuid
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("distribution", ROOT / "scripts/distribution.py")
d = importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)
mac_spec = importlib.util.spec_from_file_location("distribution_macos", ROOT / "scripts/distribution-macos.py")
mac = importlib.util.module_from_spec(mac_spec)
with patch.dict(sys.modules, {"distribution": d}):
    mac_spec.loader.exec_module(mac)


class DistributionTest(unittest.TestCase):
    def setUp(self):
        # No secrets: use a workspace fixture without Windows private-ACL changes.
        self.root = ROOT / ("build-distribution-test-" + uuid.uuid4().hex)
        self.root.mkdir()
        self.addCleanup(shutil.rmtree, self.root)
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.manifest = d.load_manifest(ROOT / "packaging/sources.json")

    def archive(self, name="fixture.tar.gz", entries=None):
        path = self.cache / name
        if entries is None:
            entries = {
                "src/LICENSE": "Original license body\nCopyright fixture\n",
                "src/src/3rdparty/example/qt_attribution.json": json.dumps({
                    "Name": "Fixture", "Copyright": "Example holder", "LicenseFile": "terms.txt"}),
                "src/src/3rdparty/example/terms.txt": "Full license terms\n",
                "src/src/3rdparty/example/header.h": "/* Copyright holder. MIT license */\nint not_a_notice;\n",
            }
        with tarfile.open(path, "w:gz") as archive:
            for filename, content in entries.items():
                data = content.encode() if isinstance(content, str) else content
                item = tarfile.TarInfo(filename)
                item.size = len(data)
                archive.addfile(item, io.BytesIO(data))
        return path

    def entry(self, archive, group="qt"):
        return {"id": group + "-fixture", "group": group,
                "url": "https://example.invalid/" + archive.name, "sha256": d.digest(archive)}

    def ifw_output(self):
        m = self.manifest
        return (f"IFW Version: {m['ifwVersion']}, built with Qt {m['ifwQtVersion']}.\n"
                f"Installer Framework SHA1: {m['ifwCommit']}\nLibarchive version: {m['ifwArchiveVersions']}\n")

    def test_manifest_is_pinned_and_https(self):
        self.assertEqual(len(self.manifest["sources"]), 10)
        for mutation in ("http://example.invalid/a.tar.gz", "https://user:pass@example.invalid/a.tar.gz"):
            manifest = json.loads(json.dumps(self.manifest))
            manifest["sources"][0]["url"] = mutation
            path = self.root / "manifest.json"
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                d.load_manifest(path)

    def test_ci_uses_the_release_qt_profile(self):
        workflow = (ROOT / ".github/workflows/windows-build.yml").read_text(encoding="utf-8")
        self.assertEqual(workflow.count(f"QT_VERSION: '{self.manifest['qtVersion']}'"), 2)
        self.assertEqual(workflow.count('python -m pip install "$env:AQTINSTALL_SOURCE"'), 2)
        self.assertIn("aqtinstall.git@076e1659807d0b362a3ed684d54c2e9c775eb9c7", workflow)

    def test_cache_hash_checked_and_no_implicit_download(self):
        archive = self.archive()
        entry = self.entry(archive)
        self.assertEqual(d.obtain(entry, self.cache), archive)
        entry["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            d.obtain(entry, self.cache)
        archive.unlink()
        with patch.object(d.urllib.request, "build_opener") as request:
            with self.assertRaisesRegex(ValueError, "explicitly"):
                d.obtain(entry, self.cache)
            request.assert_not_called()

    def test_original_metadata_body_and_header_are_preserved(self):
        text = d.collect_notices(self.archive())
        self.assertIn("Original license body\nCopyright fixture", text)
        self.assertIn('"Copyright": "Example holder"', text)
        self.assertIn("Full license terms", text)
        self.assertIn("/* Copyright holder. MIT license */", text)
        self.assertNotIn("int not_a_notice", text)

    def test_missing_license_reference_is_fatal(self):
        path = self.archive(entries={"src/qt_attribution.json": '{"LicenseFile":"missing.txt"}'})
        with self.assertRaisesRegex(ValueError, "Missing"):
                d.collect_notices(path)

    def test_binary_legal_filenames_are_not_decoded_as_notices(self):
        text = d.collect_notices(self.archive(entries={
            "src/LICENSE": "Original license body\n",
            "src/copyright.png": b"\x89PNG\r\n\x1a\n\x00binary",
            "src/LICENSES/icon.bin": b"\x00binary",
        }))
        self.assertIn("Original license body", text)
        self.assertNotIn("copyright.png", text)
        self.assertNotIn("icon.bin", text)
        with self.assertRaisesRegex(ValueError, "Non-text notice"):
            d.collect_notices(self.archive(entries={
                "src/qt_attribution.json": '{"LicenseFile":"license.bin"}',
                "src/license.bin": b"\x00binary",
            }))

    def test_archive_path_traversal_and_escaping_reference_are_rejected(self):
        for name in ("../escape", "/absolute", "C:/absolute", "x\\escape"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                d.collect_notices(self.archive(entries={name: "text"}))
        with self.assertRaises(ValueError):
            d.collect_notices(self.archive(entries={"src/qt_attribution.json": '{"LicenseFile":"../../escape"}'}))

    def test_collect_does_not_create_a_source_offer_or_overwrite_material(self):
        self.manifest["sources"] = [self.entry(self.archive())]
        output = self.root / "review"
        d.collect(self.manifest, self.cache, output)
        self.assertTrue((output / "QT-NOTICES.txt").is_file())
        self.assertFalse((output / "SOURCE-OFFER.txt").exists())
        with self.assertRaisesRegex(ValueError, "empty"):
            d.collect(self.manifest, self.cache, output)

    def test_mismatched_actual_versions_fail_closed(self):
        version = self.manifest["qtVersion"]
        d.check_versions(self.manifest, version, self.ifw_output())
        for qt, output in (("6.8.3", self.ifw_output()), (version, "IFW 4.7.0"),
                           (version, self.ifw_output().replace("6.6.0", "6.6.1")),
                           (version, self.ifw_output().replace("zlib/1.2.12", "zlib/1.3.1"))):
            with self.assertRaises(ValueError):
                d.check_versions(self.manifest, qt, output)

    def test_macos_companion_is_independent_and_binds_both_source_and_notices(self):
        manifest = d.load_manifest(ROOT / "packaging/macos/sources.json")
        self.assertEqual(manifest["profile"], "macos-official-qt-6.12.0")
        self.assertEqual(len(manifest["sources"]), 2)
        manifest["sources"] = [self.entry(self.archive())]
        qt = self.root / "qt"
        (qt / "bin").mkdir(parents=True)
        (qt / "bin/qmake").write_bytes(b"fixture; never executed")
        source = self.root / "app.zip"
        commit = "a" * 40
        with zipfile.ZipFile(source, "w") as archive:
            archive.comment = commit.encode()
            archive.writestr("LICENSE", (ROOT / "LICENSE").read_bytes())
        with patch.object(mac.subprocess, "check_output", return_value="6.12.0\n"):
            bundle = mac.prepare(manifest, self.cache, self.root / "legal", self.root / "bundles",
                                 "0.1.3", "owner/repo", commit, qt, source, "arm64")
        record = json.loads((self.root / "bundles/macos-arm64-distribution-release.json").read_text())
        self.assertEqual(record["sourceBundleSha256"], d.digest(bundle))
        self.assertEqual(record["architecture"], "arm64")
        self.assertNotIn("IFW-NOTICES.txt", record["noticeHashes"])
        with zipfile.ZipFile(bundle) as archive:
            self.assertEqual(archive.read("application-source.zip"), source.read_bytes())
            self.assertIn(b"macOS-arm64-Sources.zip", archive.read("distribution/SOURCE-OFFER.txt"))
        with patch.object(mac.subprocess, "check_output", return_value="6.11.0\n"):
            with self.assertRaisesRegex(ValueError, "profile"):
                mac.prepare(manifest, self.cache, self.root / "bad", self.root / "badbundles",
                            "0.1.3", "owner/repo", commit, qt, source, "arm64")

    def test_prepare_packages_real_bytes_and_binds_source_to_commit(self):
        self.manifest["sources"] = [self.entry(self.archive())]
        qt, ifw = self.root / "qt", self.root / "ifw"
        for root, name in ((qt, "qmake.exe"), (ifw, "installerbase.exe")):
            (root / "bin").mkdir(parents=True)
            (root / "bin" / name).write_bytes(b"fixture binary; never executed")
        source = self.root / "application.zip"
        commit = "a" * 40
        with zipfile.ZipFile(source, "w") as archive:
            archive.comment = commit.encode()
            archive.writestr("LICENSE", (ROOT / "LICENSE").read_bytes())
        output = self.root / "distribution"
        bundles = self.root / "installer"
        supplements = self.root / "supplements"
        supplements.mkdir()
        (supplements / "vendor-build.txt").write_text("fixture build information")
        with patch.object(d.subprocess, "check_output", side_effect=[self.manifest["qtVersion"] + "\n", self.ifw_output()]):
            bundle = d.prepare(self.manifest, self.cache, output, bundles, "0.1.1", "owner/repo", commit,
                               qt, ifw, source, supplements=supplements)
        with zipfile.ZipFile(bundle) as archive:
            self.assertEqual(archive.read("application-source.zip"), source.read_bytes())
            self.assertEqual(archive.read("upstream/fixture.tar.gz"), (self.cache / "fixture.tar.gz").read_bytes())
            self.assertIn(b"releases/download/v0.1.1/AgentNodify-0.1.1-Sources.zip",
                          archive.read("distribution/SOURCE-OFFER.txt"))
            self.assertEqual(archive.read("distribution/vendor-build.txt"), b"fixture build information")
        record = json.loads((bundles / "distribution-release.json").read_text())
        self.assertEqual(record["sourceBundleSha256"], hashlib.sha256(bundle.read_bytes()).hexdigest())
        self.assertEqual(record["commit"], commit)
        self.assertEqual(record["noticeHashes"]["SOURCE-OFFER.txt"], d.digest(output / "SOURCE-OFFER.txt"))
        self.assertEqual(record["supplements"]["vendor-build.txt"], d.digest(output / "vendor-build.txt"))
        self.assertIn("Requires distributor review", record["approval"])


if __name__ == "__main__":
    unittest.main()
