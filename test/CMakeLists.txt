add_subdirectory(unit)

find_package(Python3)
if(Python3_FOUND)
  file(GLOB system_tests "system/test-*.py")
  foreach(file ${system_tests})
    get_filename_component(fname ${file} NAME_WE)
    add_test(NAME "system:${fname}"
             COMMAND ${Python3_EXECUTABLE} ${file})
    set_property(TEST "system:${fname}" PROPERTY
                 ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/apps")
  endforeach()
else()
  message(WARNING "Python 3 not found. Skipping system tests.")
endif()
