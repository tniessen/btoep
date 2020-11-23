cmake_minimum_required(VERSION 3.10)

project(btoep)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

if(MSVC)
  add_compile_options(/WX /wd4996)
  add_link_options(/WX)

  # MSVC enables C5105, but the Windows SDK triggers it.
  # TODO: Remove this once popular distributions have fixed the problem
  add_compile_options(/wd5105)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(lib)
add_subdirectory(apps)

enable_testing()
list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
add_subdirectory(test/unit)
add_subdirectory(test/system)
