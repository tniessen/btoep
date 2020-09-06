if(NOT WIN32)
  set(workflow_tmp_dir "${CMAKE_CURRENT_BINARY_DIR}/workflows")
  add_test(NAME WorkflowTestSetup COMMAND mkdir "${workflow_tmp_dir}")
  add_test(NAME WorkflowTestCleanup COMMAND rm -rf "${workflow_tmp_dir}")
  set_tests_properties(WorkflowTestSetup PROPERTIES FIXTURES_SETUP WorkflowTest)
  set_tests_properties(WorkflowTestCleanup PROPERTIES FIXTURES_CLEANUP WorkflowTest)

  file(GLOB workflows "*.sh")
  foreach(workflow ${workflows})
    get_filename_component(fname ${workflow} NAME_WE)
    add_test(NAME "workflow:${fname}"
             WORKING_DIRECTORY "${workflow_tmp_dir}"
             COMMAND ${Python3_EXECUTABLE} ${workflow})
    set_tests_properties("workflow:${fname}" PROPERTIES
                         ENVIRONMENT "PATH=$<TARGET_FILE_DIR:btoep-add>"
                         FIXTURES_REQUIRED WorkflowTest
                         TIMEOUT 60)
  endforeach()
else()
  message(WARNING "Skipping workflow tests due to incompatible operating system")
endif()
