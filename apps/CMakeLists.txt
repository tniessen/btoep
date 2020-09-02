# MSVC would complain about the stack size in apps, since we allocate datasets
# on the stack. We know that we won't exceed the ~1MiB stack size limit.
# The second error code allows assignments in boolean expressions.
if(MSVC)
  add_compile_options(/wd6262 /wd6282)
endif()

file(WRITE "util/res.h" "#include <errno.h>\n\n")
SET_SOURCE_FILES_PROPERTIES("util/res.h" PROPERTIES GENERATED 1)

# Convert string resources into a header file.
file(APPEND "util/res.h" "/* String resources */\n")
file(GLOB app_strings "res/*")
foreach(app_string ${app_strings})
  file(READ ${app_string} hexdata HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hexdata ${hexdata})
  get_filename_component(varname ${app_string} NAME)
  file(APPEND "util/res.h"
       "static const char ${varname}_string[] = {${hexdata}0x00};\n")
endforeach()

function(preprocess_c_header header out_var)
  file(WRITE "${CMAKE_BINARY_DIR}/temp.c" "#include <${header}>\n")
  if(MSVC)
    set(current_search_path "${CMAKE_C_COMPILER}")
    foreach(i RANGE 8)
      if(NOT vcvarsall_bat_path)
        get_filename_component(current_search_path "${current_search_path}" DIRECTORY)
        find_file(vcvarsall_bat_path vcvarsall.bat "${current_search_path}/Auxiliary/Build")
      endif()
    endforeach()

    if(NOT vcvarsall_bat_path)
      message(FATAL_ERROR "Unable to find vcvarsall.bat for MSVC.")
    endif()

    execute_process(COMMAND "${vcvarsall_bat_path}" ${CMAKE_C_COMPILER_ARCHITECTURE_ID} && "${CMAKE_C_COMPILER}" /P /d1PP /C "${CMAKE_BINARY_DIR}/temp.c"
                    OUTPUT_QUIET)
    file(READ "${CMAKE_BINARY_DIR}/temp.i" temp_var)
    file(REMOVE "${CMAKE_BINARY_DIR}/temp.i")
  elseif(APPLE)
    execute_process(COMMAND "${CMAKE_C_COMPILER}" -isysroot "${CMAKE_OSX_SYSROOT}" -E -dD "${CMAKE_BINARY_DIR}/temp.c"
                    OUTPUT_VARIABLE temp_var)
  else()
    execute_process(COMMAND "${CMAKE_C_COMPILER}" -E -dD "${CMAKE_BINARY_DIR}/temp.c"
                    OUTPUT_VARIABLE temp_var)
  endif()
  file(REMOVE "${CMAKE_BINARY_DIR}/temp.c")
  set(${out_var} "${temp_var}" PARENT_SCOPE)
endfunction()

function(produce_error_names header func etype regex)
  preprocess_c_header(${header} preproc_h)
  file(APPEND "util/res.h" "\nstatic const char* ${func}(${etype} code) {\n  switch (code) {\n")
  string(REGEX MATCHALL "${regex}" error_matches "${preproc_h}")
  foreach(error_match ${error_matches})
    string(REGEX REPLACE "${regex}" "\\1" error_code "${error_match}")
    # Some platforms use the pseudo-code ELAST to mark the highest error code,
    # which is equal to another real error code, and would therefore lead to
    # compiler errors/warnings (due to two cases with the same value).
    if(NOT error_code STREQUAL "ELAST")
      file(APPEND "util/res.h" "#ifdef ${error_code}\n")
      file(APPEND "util/res.h" "  case ${error_code}: return \"${error_code}\";\n")
      file(APPEND "util/res.h" "#endif\n")
    endif()
  endforeach()
  file(APPEND "util/res.h" "  default: return NULL;\n  }\n}\n")
endfunction()

# This allows apps to show the name of an errno, e.g., "ENOENT". This is
# unrelated to the error message that can be retrieved using strerror etc.
option(RESOLVE_ERROR_NAMES "Resolve error names" ON)
if(RESOLVE_ERROR_NAMES)
  message(STATUS "RESOLVE_ERROR_NAMES is enabled. Please report failures.")
  message(STATUS "You can disable this feature if necessary.")
  if(MSVC)
    produce_error_names(WinError.h "get_windows_error_name" DWORD
                        "#define[ \t]+(ERROR_[^ \t\r\n]+)[ \t]+([0-9xLlUu]+)")
  endif()
  produce_error_names(errno.h "get_errno_error_name" int
                      "#define[ \t]+(E[^ \t\r\n]+)[ \t]+([0-9xLlUu]+)")
else()
  message(WARNING "RESOLVE_ERROR_NAMES is disabled. "
                  "Error names will not be available.")
  if(MSVC)
    file(APPEND "util/res.h"
         "\nstatic const char* get_windows_error_name(DWORD code) { (void) code; return NULL; }\n")
  endif()

  file(APPEND "util/res.h"
       "\nstatic const char* get_errno_error_name(int code) { (void) code; return NULL; }\n")
endif()

file(GLOB apps "btoep-*.c")
file(GLOB util_files "util/*.c")
foreach(file ${apps})
  get_filename_component(fname ${file} NAME_WE)
  add_executable(${fname} ${file} ${util_files})
  target_link_libraries(${fname} PUBLIC btoep)
  target_include_directories(${fname} PUBLIC "${PROJECT_SOURCE_DIR}/lib/include")
endforeach()
