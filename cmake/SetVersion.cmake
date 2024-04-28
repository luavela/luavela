# Find, check and set uJIT's version from ChangeLog or a VCS tag.
# Copyright (C) 2020-2024 LuaVela Authors. See Copyright Notice in COPYRIGHT
# Copyright (C) 2015-2020 IPONWEB Ltd. See Copyright Notice in COPYRIGHT

# This module exposes following variables to the project:
set(UJIT_VERSION_STRING "")
set(UJIT_VERSION_MAJOR   0)
set(UJIT_VERSION_MINOR   0)

# Reads versions from the project's ChangeLog and VCS (if applicable) and
# stores the result into version_changelog and version_vcs, respectively.
function(ReadVersions version_changelog version_vcs)
  if(EXISTS "${PROJECT_SOURCE_DIR}/ChangeLog.INTERNAL")
    set(changelog_file "${PROJECT_SOURCE_DIR}/ChangeLog.INTERNAL")
  else()
    set(changelog_file "${PROJECT_SOURCE_DIR}/ChangeLog")
  endif()

  file(STRINGS "${changelog_file}" first_line LIMIT_COUNT 1)
  string(REGEX MATCH "^version +([^ ]+)$" ver_match ${first_line})
  set(${version_changelog} ${CMAKE_MATCH_1} PARENT_SCOPE)

  set(ver_vcs "NONE")
  find_package(Git QUIET)
  if(Git_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} tag -l --points-at HEAD
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      OUTPUT_VARIABLE vcs_tag)
    string(STRIP "${vcs_tag}" ver_vcs)
    string(REPLACE "\n" ";" ver_vcs "${ver_vcs}")

    if("${ver_vcs}" STREQUAL "")
      set(ver_vcs "NONE")
    else()
      # Check that HEAD has less than 2 tags
      list(LENGTH ver_vcs number_of_tags)
      if (${number_of_tags} GREATER 1)
        message(FATAL_ERROR "[SetVersion] HEAD has several tags: ${ver_vcs}")
      endif()
    endif()
  endif()
  set(${version_vcs} ${ver_vcs} PARENT_SCOPE)
endfunction()

# Matches version_string against the version regex.
# Throws an error if it does not match. Otherwise populates variables:
# * version_major: Major version as a number.
# * version_minor: Minor version as a number.
# * version_type : Version type as a string. Possible values: stable, dev.
# Valid version examples:
# * stable: 0.15.0, 23.4.56
# * dev   : 0.15-dev0, 1.2-dev34
function(ParseVersionString version_major version_minor version_type version_string)
  if(version_string MATCHES "^([0-9]+)\\.([0-9]+)(\\.|-dev)[0-9]+$")
    set(${version_major} ${CMAKE_MATCH_1} PARENT_SCOPE)
    set(${version_minor} ${CMAKE_MATCH_2} PARENT_SCOPE)
    if(CMAKE_MATCH_3 STREQUAL ".")
      set(${version_type} "stable" PARENT_SCOPE)
    elseif(CMAKE_MATCH_3 STREQUAL "-dev")
      set(${version_type} "dev" PARENT_SCOPE)
    endif()
  else()
    message(FATAL_ERROR "[SetVersion] Malformed version string '${version_string}'")
  endif()
endfunction()

# For stable build from VCS, ensures correct layout:
# * Version read from ChangeLog must match version read from the VCS.
# * Version x.y... must belong to the x.y-release branch in the VCS.
function(CheckVCSLayout version_changelog version_vcs version_major version_minor)
  if(NOT version_changelog STREQUAL version_vcs)
    message(FATAL_ERROR "[SetVersion] ChangeLog version ${version_changelog} does not match VCS version ${version_vcs}")
  endif()

  set(expected_vcs_branch "${version_major}.${version_minor}-release")

  find_package(Git QUIET REQUIRED)

  # Get HEAD commit id
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE head_commit_id)
  string(STRIP "${head_commit_id}" head_commit_id)

  # Get list of branches HEAD is reachable from
  execute_process(
    COMMAND ${GIT_EXECUTABLE} branch -a --contains ${head_commit_id}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE containing_branches)

  # Check that HEAD doesn't belong to 'master' or feature branch
  if("${containing_branches}" MATCHES "master\n")
    string(CONCAT error_msg "[SetVersion] HEAD (${head_commit_id}) belongs to 'master' or a feature branch,"
                            " was expected to belong to the branch '${expected_vcs_branch}'")
    message(FATAL_ERROR ${error_msg})
  endif()

  # Check that HEAD is reachable from ${expected_vcs_branch}
  if(NOT "${containing_branches}" MATCHES "${expected_vcs_branch}\n")
    message(FATAL_ERROR "[SetVersion] HEAD (${head_commit_id}) doesn't belong to the branch '${expected_vcs_branch}'")
  endif()
endfunction()

#
# Main logic
#

function(SetVersion version_string version_major version_minor)
  set(ver_changelog "NONE")
  set(ver_vcs       "NONE")
  set(ver_string    "")
  set(ver_major      0)
  set(ver_minor      0)
  set(ver_type      "")

  ReadVersions(ver_changelog ver_vcs)

  if(ver_vcs STREQUAL "NONE")
    message(STATUS "[SetVersion] Reading version from ChangeLog: ${ver_changelog}")
    set(ver_string ${ver_changelog})
  else()
    message(STATUS "[SetVersion] Reading version from VCS: ${ver_vcs}")
    set(ver_string ${ver_vcs})
  endif()

  ParseVersionString(ver_major ver_minor ver_type "${ver_string}")

  if(ver_type STREQUAL "stable" AND (NOT ver_vcs STREQUAL "NONE"))
    CheckVCSLayout(${ver_changelog} ${ver_vcs} ${ver_major} ${ver_minor})
  endif()

  set(${version_string} ${ver_string} PARENT_SCOPE)
  set(${version_major} ${ver_major} PARENT_SCOPE)
  set(${version_minor} ${ver_minor} PARENT_SCOPE)
endfunction()

SetVersion(UJIT_VERSION_STRING UJIT_VERSION_MAJOR UJIT_VERSION_MINOR)

message(STATUS "[SetVersion] Found version ${UJIT_VERSION_STRING}")

configure_file(
  src/ujit.h.in
  src/ujit.h
  @ONLY ESCAPE_QUOTES
)
