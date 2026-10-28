# Packaging is opt-in: ordinary builds do not require IFW or deploy dependencies.
include(GNUInstallDirs)
if(NOT CMAKE_INSTALL_BINDIR STREQUAL "bin")
  message(FATAL_ERROR "AgentNodify's installer currently requires CMAKE_INSTALL_BINDIR=bin")
endif()
if(UNIX AND NOT APPLE AND Qt6_VERSION VERSION_LESS 6.7)
  message(FATAL_ERROR "Linux installer deployment requires Qt 6.7 or newer")
endif()

set(AGENTNODIFY_LEGAL_DIRECTORY "" CACHE PATH
  "Additional distribution notices, runtime licenses and source availability information")
if(AGENTNODIFY_LEGAL_DIRECTORY AND NOT IS_DIRECTORY "${AGENTNODIFY_LEGAL_DIRECTORY}")
  message(FATAL_ERROR "AGENTNODIFY_LEGAL_DIRECTORY must be an existing directory")
endif()

# The app and its dependencies form one versioned component. Never copy build/Release.
qt_generate_deploy_app_script(TARGET AgentNodify OUTPUT_SCRIPT _deploy_script
  NO_TRANSLATIONS NO_COMPILER_RUNTIME)
install(SCRIPT "${_deploy_script}" COMPONENT AgentNodify)

if(MSVC)
  # App-local deployment avoids a separate, elevated VC++ redistributable installer.
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  set(CMAKE_INSTALL_DEBUG_LIBRARIES FALSE)
  include(InstallRequiredSystemLibraries)
  if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    message(FATAL_ERROR "MSVC redistributable DLLs not found; use a Visual Studio build environment")
  endif()
  install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
    DESTINATION bin COMPONENT AgentNodify)
endif()

install(DIRECTORY examples/ DESTINATION examples COMPONENT AgentNodify
  FILES_MATCHING PATTERN "*.json" PATTERN "*.py" PATTERN "*.js")
install(FILES "${json_SOURCE_DIR}/LICENSE.MIT"
  DESTINATION licenses/nlohmann-json COMPONENT AgentNodify)
install(FILES "${json_validator_SOURCE_DIR}/LICENSE"
  DESTINATION licenses/json-schema-validator COMPONENT AgentNodify)
install(FILES "${qtnodes_SOURCE_DIR}/LICENSE.rst"
  DESTINATION licenses/QtNodes COMPONENT AgentNodify)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.txt"
  DESTINATION licenses COMPONENT AgentNodify)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/INSTALL-NOTES.txt"
  DESTINATION . COMPONENT AgentNodify)
if(AGENTNODIFY_LEGAL_DIRECTORY)
  install(DIRECTORY "${AGENTNODIFY_LEGAL_DIRECTORY}/"
    DESTINATION licenses/distribution COMPONENT AgentNodify)
else()
  message(WARNING "No distribution legal directory supplied. Before public distribution, supply Qt/IFW and third-party licenses and corresponding-source information; see spec/packaging.md.")
endif()

set(CPACK_GENERATOR IFW)
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/CPackOptions.cmake")
set(CPACK_PACKAGE_NAME AgentNodify)
set(CPACK_PACKAGE_VENDOR AgentNodify)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AgentNodify workflow editor")
set(CPACK_PACKAGE_FILE_NAME "AgentNodify-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-Setup")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/installer")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_COMPONENTS_ALL AgentNodify)
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_IFW_PACKAGE_TITLE "AgentNodify Setup")
set(CPACK_IFW_PACKAGE_NAME AgentNodify)
set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY AgentNodify)
set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME MaintenanceTool)
set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/controlscript.qs")
set(CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS ON)
set(CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH ON)
# Do not recursively delete unknown files a user may have added to the directory.
set(CPACK_IFW_PACKAGE_REMOVE_TARGET_DIR OFF)
if(WIN32)
  # The controller resolves LOCALAPPDATA at installer runtime (not build time).
  set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/AppData/Local/Programs/AgentNodify")
elseif(APPLE)
  set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/Applications/AgentNodify")
else()
  set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/.local/opt/AgentNodify")
endif()

include(CPack)
include(CPackIFW)
if(NOT CPACK_IFW_BINARYCREATOR_EXECUTABLE OR NOT CPACK_IFW_INSTALLERBASE_EXECUTABLE)
  message(FATAL_ERROR "Qt Installer Framework tools not found. Set CPACK_IFW_ROOT or QTIFWDIR to the IFW directory (not its bin directory).")
endif()
if(CPACK_IFW_FRAMEWORK_VERSION VERSION_LESS 4.0)
  message(FATAL_ERROR "Qt Installer Framework 4.0 or newer is required")
endif()
cpack_add_component(AgentNodify DISPLAY_NAME AgentNodify
  DESCRIPTION "Workflow editor, Qt runtime, examples and license notices" REQUIRED)
cpack_ifw_configure_component(AgentNodify
  NAME org.agentnodify.application
  VERSION "${PROJECT_VERSION}"
  FORCED_INSTALLATION
  SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installscript.qs")

add_test(NAME packaging_layout COMMAND "${CMAKE_COMMAND}"
  "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
  "-DCONFIGURATION=$<CONFIG>"
  "-DHOST_WINDOWS=${WIN32}"
  "-DHOST_APPLE=${APPLE}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/packaging_test.cmake")
set_tests_properties(packaging_layout PROPERTIES TIMEOUT 180)
