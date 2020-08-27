# MSVC would complain about the stack size in apps, since we allocate datasets
# on the stack. We know that we won't exceed the ~1MiB stack size limit.
# The second error code allows assignments in boolean expressions.
if(MSVC)
  add_compile_options(/wd6262 /wd6282)
endif()

file(GLOB apps "btoep-*.c")
file(GLOB util_files "util/*.c")
foreach(file ${apps})
  get_filename_component(fname ${file} NAME_WE)
  add_executable(${fname} ${file} ${util_files})
  target_link_libraries(${fname} PUBLIC btoep)
  target_include_directories(${fname} PUBLIC "${PROJECT_SOURCE_DIR}/lib/include")
endforeach()
