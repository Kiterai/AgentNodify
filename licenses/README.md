# License scope and provenance

AgentNodify's original code, documentation, examples and project-owned assets are
licensed under the MIT license in ../LICENSE. The copyright notice identifies
the AgentNodify contributors collectively; it does not transfer ownership of
individual contributions. Third-party software, notices and license texts keep
their own copyright and licenses and are not relicensed under MIT.

## Included texts

The following texts were copied verbatim from the upstream sources. Keep the
copyright notices and license terms intact. CMake checks the three fixed-version
library notices against the source actually used for the build (normalizing line
endings). Update these files and the inventory when upgrading dependencies.

| Local file | Upstream source |
|---|---|
| QtNodes/LICENSE.rst | https://github.com/paceholder/nodeeditor/blob/3.0.12/LICENSE.rst |
| nlohmann-json/LICENSE.MIT | https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT |
| json-schema-validator/LICENSE | https://github.com/pboettch/json-schema-validator/blob/2.3.0/LICENSE |
| Qt/LGPL-3.0-only.txt | https://github.com/qt/qtbase/blob/v6.8.3/LICENSES/LGPL-3.0-only.txt |
| Qt/GPL-3.0-only.txt | https://github.com/qt/qtbase/blob/v6.8.3/LICENSES/GPL-3.0-only.txt |
| IFW/LICENSE.GPL3-EXCEPT | https://github.com/qtproject/installer-framework/blob/4.7/LICENSE.GPL3-EXCEPT |

The supplementary HEDLEY-NOTICE.txt preserves the SPDX notices in the bundled
Hedley header. SMTP-LICENSE.txt reproduces the MIT notice in the bundled SMTP
address validator. Their source locations are recorded in those files.

The Qt license texts are standard license documents, not a claim that every build
uses Qt 6.8.3. The application displays its compile-time and runtime Qt versions.
The IFW text corresponds to the 4.7 series used by the release workflow; verify
the applicable terms when using a different toolkit or commercial distribution.

## Qt and IFW are not covered by the application's MIT license

The Qt Core, Gui and Widgets libraries used by the application, and the Network
and Svg libraries/plugins that may be added during deployment, are available
under LGPLv3/GPLv3/commercial terms in the relevant Qt distributions. The intended
open-source application distribution uses the LGPLv3 option for applicable
modules, with dynamically linked libraries. Qt Test is used by tests and is not
part of the application payload. Verify additional plugins and modules before
redistribution; this statement does not cover GPL-only modules.

Qt is copyright The Qt Company Ltd. and other contributors. Qt and related
names/logos are trademarks of their respective owners. Qt's bundled third-party
code has additional copyright notices and licenses.

IFW is separate software used to build the installer and run MaintenanceTool.
The open-source distribution uses GPLv3 with the Qt Company GPL Exception 1.0.
The combined license/exception text is included here. Its embedded Qt and other
third-party code must be considered separately from the application's Qt DLLs.

## Corresponding source and distribution-specific notices

These license texts and upstream URLs alone do not fulfill all binary
redistribution obligations. The distributor must supply the notices and
corresponding-source access required for the exact Qt/IFW binaries and bundled
third-party code, including applicable modifications and build information.
Keep the LGPL rights to modify/replace covered libraries and debug those changes;
do not impose contrary restrictions in an application EULA.

Place reviewed, build-specific notices and actual source availability information
in the directory passed as AGENTNODIFY_LEGAL_DIRECTORY (-LegalDirectory in the
packaging script). Text files there are also embedded in the application's
license viewer; all supplied files are installed in licenses/distribution/.
Do not put credentials or private logs there.

No completed source offer, download location, or legal approval is asserted by
this repository's generic notices. The release workflow's legal-approval and
required-document checks remain in force. See ../spec/packaging.md.

For the pinned Windows release profile, scripts/distribution.py collects original
texts from checksum-verified source archives. Its collect command creates review
material only; prepare also checks actual tool versions, bundles the source
archives and git-archived application source, and generates per-release source
access information. The publisher verifies all asset uploads before making a
release public. This automation does not replace review of vendor patches,
build configuration, exact source correspondence or continuing source access.

Microsoft runtime files, when distributed, remain subject to Microsoft's
redistribution terms. Externally installed agent CLIs, Python and Node.js are
not bundled or relicensed by AgentNodify.
