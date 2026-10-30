# Legal texts are available in standalone executables as well as IFW packages.
set(AGENTNODIFY_LEGAL_DIRECTORY "" CACHE PATH
  "Additional distribution notices, runtime licenses and source availability information")
if(AGENTNODIFY_LEGAL_DIRECTORY AND NOT IS_DIRECTORY "${AGENTNODIFY_LEGAL_DIRECTORY}")
  message(FATAL_ERROR "AGENTNODIFY_LEGAL_DIRECTORY must be an existing directory")
endif()

function(agentnodify_verify_license upstream copy)
  file(READ "${upstream}" _upstream)
  file(READ "${copy}" _copy)
  string(REPLACE "\r\n" "\n" _upstream "${_upstream}")
  string(REPLACE "\r\n" "\n" _copy "${_copy}")
  if(NOT _upstream STREQUAL _copy)
    message(FATAL_ERROR "License text mismatch: ${copy}; review dependency notices before building")
  endif()
endfunction()
agentnodify_verify_license("${json_SOURCE_DIR}/LICENSE.MIT"
  "${CMAKE_CURRENT_SOURCE_DIR}/licenses/nlohmann-json/LICENSE.MIT")
agentnodify_verify_license("${json_validator_SOURCE_DIR}/LICENSE"
  "${CMAKE_CURRENT_SOURCE_DIR}/licenses/json-schema-validator/LICENSE")
agentnodify_verify_license("${qtnodes_SOURCE_DIR}/LICENSE.rst"
  "${CMAKE_CURRENT_SOURCE_DIR}/licenses/QtNodes/LICENSE.rst")
agentnodify_verify_license("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/distribution/APPLICATION-LICENSE.txt")

set(_license_files
  LICENSE
  packaging/THIRD-PARTY-NOTICES.txt
  licenses/README.md
  licenses/Qt/LGPL-3.0-only.txt
  licenses/Qt/GPL-3.0-only.txt
  licenses/IFW/LICENSE.GPL3-EXCEPT
  licenses/QtNodes/LICENSE.rst
  licenses/nlohmann-json/LICENSE.MIT
  licenses/nlohmann-json/HEDLEY-NOTICE.txt
  licenses/json-schema-validator/LICENSE
  licenses/json-schema-validator/SMTP-LICENSE.txt)
set(_distribution_texts)
if(AGENTNODIFY_LEGAL_DIRECTORY)
  file(GLOB_RECURSE _distribution_texts CONFIGURE_DEPENDS
    "${AGENTNODIFY_LEGAL_DIRECTORY}/*.txt"
    "${AGENTNODIFY_LEGAL_DIRECTORY}/*.md"
    "${AGENTNODIFY_LEGAL_DIRECTORY}/*.rst")
  foreach(_file IN LISTS _distribution_texts)
    file(RELATIVE_PATH _relative "${AGENTNODIFY_LEGAL_DIRECTORY}" "${_file}")
    set_source_files_properties("${_file}" PROPERTIES QT_RESOURCE_ALIAS "distribution/${_relative}")
    list(APPEND _license_files "${_file}")
  endforeach()
endif()
list(LENGTH _distribution_texts AGENTNODIFY_DISTRIBUTION_TEXT_COUNT)
qt_add_resources(agentui application_licenses
  PREFIX "/legal" BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${_license_files})

install(FILES LICENSE DESTINATION . COMPONENT AgentNodify)
install(DIRECTORY licenses/ DESTINATION licenses COMPONENT AgentNodify)
install(FILES packaging/THIRD-PARTY-NOTICES.txt DESTINATION licenses COMPONENT AgentNodify)
