# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\ZenithGroundStationQt_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ZenithGroundStationQt_autogen.dir\\ParseCache.txt"
  "ZenithGroundStationQt_autogen"
  )
endif()
