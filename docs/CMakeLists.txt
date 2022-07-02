find_program(DOXYGEN doxygen)
if (NOT DOXYGEN)
    message(STATUS "Documentation generation disabled (doxygen not found)")
    return()
endif()

find_program(MKDOCS mkdocs)
if (NOT MKDOCS)
    message(STATUS "Documentation generation disabled (mkdocs not found)")
    return()
endif()

set(lokinet_doc_sources "${DOCS_SRC}")
string(REPLACE ";" " " lokinet_doc_sources_spaced "${lokinet_doc_sources}")

add_custom_target(clean_xml COMMAND ${CMAKE_COMMAND} -E rm -rf doxyxml)
add_custom_target(clean_markdown COMMAND ${CMAKE_COMMAND} -E rm -rf markdown)

add_custom_command(
    OUTPUT doxyxml/index.xml
    COMMAND ${DOXYGEN} Doxyfile
    DEPENDS
    clean_xml
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        ${lokinet_doc_sources}
)

# find doxybook2
find_program(DOXYBOOK2 doxybook2)
if(NOT DOXYBOOK2)
  if(NOT DOXYBOOK2_ZIP_URL)
    set(DOXYBOOK2_VERSION v1.4.0 CACHE STRING "doxybook2 version")
    set(DOXYBOOK2_ZIP_URL "https://github.com/matusnovak/doxybook2/releases/download/${DOXYBOOK2_VERSION}/doxybook2-linux-amd64-${DOXYBOOK2_VERSION}.zip")
    set(DOXYBOOK2_ZIP_HASH_OPTS EXPECTED_HASH SHA256=bab9356f5daa550cbf21d8d9b554ea59c8be039716a2caf6e96dee52713fccb0)
  endif()

  file(DOWNLOAD
    ${DOXYBOOK2_ZIP_URL}
    ${CMAKE_CURRENT_BINARY_DIR}/doxybook2.zip
    ${DOXYBOOK2_ZIP_HASH_OPTS})

  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_CURRENT_BINARY_DIR}/doxybook2.zip
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set(DOXYBOOK2 ${CMAKE_CURRENT_BINARY_DIR}/bin/doxybook2)
  set(doxybook_localbin ${DOXYBOOK2})
endif()

add_custom_command(
    OUTPUT gen
    COMMAND ${DOXYBOOK2} --input ${CMAKE_CURRENT_BINARY_DIR}/doxyxml --output ${CMAKE_CURRENT_BINARY_DIR}/gen --config config.json
    DEPENDS
    ${doxybook_localbin}
    ${CMAKE_CURRENT_BINARY_DIR}/gen/index.md
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/doxyxml/index.xml)

add_custom_target(clean_html COMMAND ${CMAKE_COMMAND} -E rm -rf html)

add_custom_command(
  OUTPUT markdown
  COMMAND find ${CMAKE_CURRENT_BINARY_DIR}/gen/ -type f -name '*.md' -exec ${CMAKE_CURRENT_SOURCE_DIR}/fix-markdown.sh {} "\;" && ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/gen ${CMAKE_CURRENT_BINARY_DIR}/markdown
  DEPENDS gen
)

add_custom_command(
  OUTPUT html
  COMMAND ${MKDOCS} build
  DEPENDS
  clean_html
  ${CMAKE_CURRENT_BINARY_DIR}/markdown)

add_custom_target(doc DEPENDS markdown)

configure_file(Doxyfile.in Doxyfile @ONLY)
configure_file(index.md.in index.md @ONLY)

configure_file(config.json config.json COPYONLY)
configure_file(mkdocs.yml mkdocs.yml COPYONLY)

# we seperate this step out so we force clean_markdown to run before markdown target
add_custom_command(
  OUTPUT gen/index.md
  COMMAND ${CMAKE_COMMAND} -E copy index.md gen/index.md
  DEPENDS clean_markdown)
