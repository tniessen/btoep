set(unit_test_tmp_dir "${CMAKE_CURRENT_BINARY_DIR}/tmp")

add_test(NAME UnitTestSetup COMMAND mkdir "${unit_test_tmp_dir}")
set_tests_properties(UnitTestSetup PROPERTIES FIXTURES_SETUP UnitTest)
add_test(NAME UnitTestCleanup COMMAND rm -rf "${unit_test_tmp_dir}")
set_tests_properties(UnitTestCleanup PROPERTIES FIXTURES_CLEANUP UnitTest)

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
