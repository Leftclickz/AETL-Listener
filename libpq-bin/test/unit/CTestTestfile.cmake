# CMake generated Testfile for 
# Source directory: C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit
# Build directory: C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(unit_runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/unit/Debug/unit_runner.exe")
  set_tests_properties(unit_runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;56;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(unit_runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/unit/Release/unit_runner.exe")
  set_tests_properties(unit_runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;56;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(unit_runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/unit/MinSizeRel/unit_runner.exe")
  set_tests_properties(unit_runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;56;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;0;")
elseif("${CTEST_CONFIGURATION_TYPE}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(unit_runner "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpq-bin/test/unit/RelWithDebInfo/unit_runner.exe")
  set_tests_properties(unit_runner PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;56;add_test;C:/Users/ceecam/source/git repos/AETL-Generator/AETL-Generator/libpqxx/test/unit/CMakeLists.txt;0;")
else()
  add_test(unit_runner NOT_AVAILABLE)
endif()
