# CMake generated Testfile for 
# Source directory: C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test
# Build directory: C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/Debug/runner.exe")
  set_tests_properties(runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;93;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/Release/runner.exe")
  set_tests_properties(runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;93;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/MinSizeRel/runner.exe")
  set_tests_properties(runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;93;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/RelWithDebInfo/runner.exe")
  set_tests_properties(runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;93;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/CMakeLists.txt;0;")
else()
  add_test(runner NOT_AVAILABLE)
endif()
subdirs("unit")
