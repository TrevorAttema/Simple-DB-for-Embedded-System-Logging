# CMakeList.txt : CMake project for testapp, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (testapp "testapp.cpp" "testapp.h" "FileHander_Windows.cpp" "dbengine.h" "dbengine.cpp" "dbengine.index.cpp" "Instrumentation.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET testapp PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
