
set(ABYSS src)

set(ABYSS_LIB abyss)

include_directories(include)

set(ABYSS_SRC
  ${ABYSS}/md5.cpp
  ${ABYSS}/http.cpp
  ${ABYSS}/client.cpp
  ${ABYSS}/server.cpp
  ${ABYSS}/json.cpp)

add_library(${ABYSS_LIB} ${ABYSS_SRC})
