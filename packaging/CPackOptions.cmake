# CPack can also be invoked directly, bypassing scripts/package.ps1.
if(NOT CPACK_BUILD_CONFIG STREQUAL "Release")
  message(FATAL_ERROR "Generate the installer with cpack -C Release; Debug runtimes must not be distributed")
endif()
