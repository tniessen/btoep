find_package(Python3)
if(Python3_FOUND)
  if(Python3_VERSION VERSION_GREATER_EQUAL "3.7")
    file(GLOB system_tests "test-*.py")
    foreach(file ${system_tests})
      get_filename_component(fname ${file} NAME_WE)
      add_test(NAME "system:${fname}"
               COMMAND ${Python3_EXECUTABLE} ${file})
      set_tests_properties("system:${fname}" PROPERTIES
                           ENVIRONMENT "PATH=$<TARGET_FILE_DIR:btoep-add>"
                           TIMEOUT 60)
    endforeach()
  else()
    message(WARNING "Python ${Python3_VERSION} found, but need version 3.7 or newer.")
  endif()
else()
  message(WARNING "Python 3 not found. Skipping system tests.")
endif()
