set(unit_test_tmp_dir "${CMAKE_CURRENT_BINARY_DIR}/tmp")

if(MSVC)
  add_test(NAME UnitTestSetup COMMAND powershell -Command mkdir "${unit_test_tmp_dir}")
  add_test(NAME UnitTestCleanup COMMAND powershell -Command rmdir -recurse "${unit_test_tmp_dir}")
else()
  add_test(NAME UnitTestSetup COMMAND mkdir "${unit_test_tmp_dir}")
  add_test(NAME UnitTestCleanup COMMAND rm -rf "${unit_test_tmp_dir}")
endif()
set_tests_properties(UnitTestSetup PROPERTIES FIXTURES_SETUP UnitTest)
set_tests_properties(UnitTestCleanup PROPERTIES FIXTURES_CLEANUP UnitTest)

# MSVC would complain about the stack size in tests, since we allocate datasets
# on the stack. We know that we won't exceed the ~1MiB stack size limit.
if(MSVC)
  add_compile_options(/wd6262)
endif()

add_compile_options(-UNDEBUG)  # Necessary for tests to work in release builds

file(GLOB unit_tests "test-*.c")
foreach(file ${unit_tests})
  get_filename_component(fname ${file} NAME_WE)
  add_executable(${fname} ${file})
  target_link_libraries(${fname} PUBLIC btoep)
  target_include_directories(${fname} PUBLIC "${PROJECT_SOURCE_DIR}/lib/include")
  add_test(NAME "unit:${fname}"
           WORKING_DIRECTORY "${unit_test_tmp_dir}"
           COMMAND $<TARGET_FILE:${fname}>)
  set_tests_properties("unit:${fname}" PROPERTIES
                       FIXTURES_REQUIRED UnitTest
                       TIMEOUT 20)
endforeach()
