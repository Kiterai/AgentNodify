cmake_minimum_required(VERSION 3.24)
if(NOT CONFIGURATION STREQUAL "Release")
  message(FATAL_ERROR "Installer validation must use a Release build")
endif()
# Use a new directory each time: stale files cannot hide missing deployment files.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _suffix)
set(_stage "${BUILD_DIRECTORY}/packaging-check-${_suffix}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
  --config "${CONFIGURATION}" --prefix "${_stage}" --component AgentNodify
  RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Installer staging failed: ${_result}")
endif()
set(_required
  INSTALL-NOTES.txt licenses/THIRD-PARTY-NOTICES.txt
  licenses/nlohmann-json/LICENSE.MIT licenses/json-schema-validator/LICENSE
  licenses/QtNodes/LICENSE.rst examples/script.flow.json)
if(HOST_WINDOWS)
  list(APPEND _required bin/AgentNodify.exe bin/Qt6Core.dll bin/Qt6Gui.dll
    bin/Qt6Widgets.dll bin/qt.conf plugins/platforms/qwindows.dll
    bin/vcruntime140.dll bin/msvcp140.dll)
elseif(HOST_APPLE)
  list(APPEND _required AgentNodify.app/Contents/MacOS/AgentNodify)
else()
  list(APPEND _required bin/AgentNodify)
endif()
foreach(_file IN LISTS _required)
  if(NOT EXISTS "${_stage}/${_file}")
    message(FATAL_ERROR "Missing distribution file: ${_file}")
  endif()
endforeach()
file(GLOB_RECURSE _files RELATIVE "${_stage}" "${_stage}/*")
foreach(_file IN LISTS _files)
  if(_file MATCHES "(^|/)(fake_[^/]*|agentnodify_[^/]*|workspace\\.json|instance\\.lock)$"
      OR _file MATCHES "\\.(lib|pdb|obj)$"
      OR _file MATCHES "(^|/)(history|smoke-instance|test-results)/")
    message(FATAL_ERROR "Development or user file included in distribution: ${_file}")
  endif()
endforeach()
if(HOST_WINDOWS)
  # Do not accidentally resolve Qt or compiler DLLs from the developer's PATH.
  set(ENV{PATH} "$ENV{SystemRoot}/System32;$ENV{SystemRoot}")
  unset(ENV{QT_PLUGIN_PATH})
  unset(ENV{QT_QPA_PLATFORM_PLUGIN_PATH})
  set(ENV{QT_QPA_PLATFORM} "windows")
  execute_process(COMMAND "${_stage}/bin/AgentNodify.exe" --smoke-test
    RESULT_VARIABLE _smoke_result TIMEOUT 30)
  if(NOT _smoke_result STREQUAL "0")
    message(FATAL_ERROR "Staged application smoke test failed: ${_smoke_result}")
  endif()
endif()
message(STATUS "Installer staging validated: ${_stage}")
