# MSVC would complain about the stack size in apps, since we allocate datasets
# on the stack. We know that we won't exceed the ~1MiB stack size limit.
# The second error code allows assignments in boolean expressions.
if(MSVC)
  add_compile_options(/wd6262 /wd6282)
endif()

file(WRITE "util/res.h" "")
file(GLOB app_strings "res/*")
foreach(app_string ${app_strings})
  file(READ ${app_string} hexdata HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hexdata ${hexdata})
  get_filename_component(varname ${app_string} NAME)
  file(APPEND "util/res.h"
       "static const char ${varname}_string[] = {${hexdata}0x00};\n")
endforeach()

file(GLOB apps "btoep-*.c")
file(GLOB util_files "util/*.c")
foreach(file ${apps})
  get_filename_component(fname ${file} NAME_WE)
  add_executable(${fname} ${file} ${util_files})
  target_link_libraries(${fname} PUBLIC btoep)
  target_include_directories(${fname} PUBLIC "${PROJECT_SOURCE_DIR}/lib/include")
endforeach()
