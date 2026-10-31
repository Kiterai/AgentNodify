"""Offline packaging guards: no installation, signing, networking or credentials."""
import importlib.util
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


package = module("package_macos", ROOT / "scripts/package-macos.py")
validator = module("test_macos_installer", ROOT / "scripts/test-macos-installer.py")


class MacPackagingTest(unittest.TestCase):
    def test_never_install_on_developer_host(self):
        with patch.dict(os.environ, {}, clear=True), patch.object(validator, "run") as run:
            with self.assertRaisesRegex(ValueError, "disposable GitHub"):
                validator.roundtrip(Path("fixture.pkg"), "0.1.0")
            run.assert_not_called()

    def test_stable_version_required(self):
        with patch.object(package.platform, "system", return_value="Darwin"), patch.object(package, "run") as run:
            for value in ["0.1", "1.0.0-beta", "01.0.0", "../bad", "1.0.0;echo"]:
                with self.assertRaisesRegex(ValueError, "major.minor.patch"):
                    package.package(Path("missing"), Path("missing"), value)
            run.assert_not_called()

    def test_external_dependencies_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / "AgentNodify.app"
            binary = app / "Contents/MacOS/AgentNodify"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"\xcf\xfa\xed\xfe")
            with patch.object(package, "dependencies", return_value=["/opt/local/lib/foreign.dylib"]):
                with self.assertRaisesRegex(ValueError, "External dependency"):
                    package.repair_and_verify_dependencies(app)
            with patch.object(package, "dependencies", return_value=["@rpath/missing.dylib"]):
                with self.assertRaisesRegex(ValueError, "Unresolved bundled"):
                    package.repair_and_verify_dependencies(app)

    def test_relocate_only_bundled_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / "AgentNodify.app"
            binary = app / "Contents/Frameworks/libdecoder.dylib"
            binary.parent.mkdir(parents=True)
            binary.write_bytes(b"\xcf\xfa\xed\xfe")
            target = binary.parent / "libcommon.dylib"
            target.write_bytes(b"fixture")
            with patch.object(package, "dependencies", return_value=["@rpath/libcommon.dylib"]), patch.object(package, "run") as run:
                package.repair_and_verify_dependencies(app)
                run.assert_called_once_with("/usr/bin/install_name_tool", "-change", "@rpath/libcommon.dylib", "@loader_path/libcommon.dylib", binary)
            target.unlink()
            target.symlink_to(Path(directory) / "outside.dylib")
            (Path(directory) / "outside.dylib").write_bytes(b"fixture")
            with patch.object(package, "dependencies", return_value=["@rpath/libcommon.dylib"]):
                with self.assertRaisesRegex(ValueError, "Unresolved bundled"):
                    package.repair_and_verify_dependencies(app)

    def test_install_ids_are_not_runtime_dependencies(self):
        binary = Path("fixture.dylib")
        responses = [subprocess.CompletedProcess([], 0, "fixture:\n\t/private/kit/fixture.dylib (compatibility version 1)\n\t/usr/lib/libSystem.B.dylib (compatibility version 1)\n"),
                     subprocess.CompletedProcess([], 0, "fixture:\n/private/kit/fixture.dylib\n")]
        with patch.object(package, "run", side_effect=responses):
            self.assertEqual(package.dependencies(binary), ["/usr/lib/libSystem.B.dylib"])


if __name__ == "__main__":
    unittest.main()
