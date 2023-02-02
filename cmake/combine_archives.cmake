function(combine_archives output_archive)
  set(FULL_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib${output_archive}.a)
  set(output_archive_dummy_file ${CMAKE_CURRENT_BINARY_DIR}/${output_archive}.dummy.cpp)
  add_custom_command(OUTPUT ${output_archive_dummy_file}
                     COMMAND touch ${output_archive_dummy_file}
                     DEPENDS ${ARGN})
  add_library(${output_archive} STATIC EXCLUDE_FROM_ALL ${output_archive_dummy_file})

  if(NOT APPLE)
    set(mri_file ${CMAKE_CURRENT_BINARY_DIR}/${output_archive}.mri)
    set(mri_content "create ${FULL_OUTPUT_PATH}\n")
    foreach(in_archive ${ARGN})
        string(APPEND mri_content "addlib $<TARGET_FILE:${in_archive}>\n")
    endforeach()
    string(APPEND mri_content "save\nend\n")
    file(GENERATE OUTPUT ${mri_file} CONTENT "${mri_content}")

    add_custom_command(TARGET ${output_archive}
                       POST_BUILD
                       COMMAND ar -M < ${mri_file})
  else()
    set(merge_libs)
    foreach(in_archive ${ARGN})
      list(APPEND merge_libs $<TARGET_FILE:${in_archive}>)
    endforeach()
    add_custom_command(TARGET ${output_archive}
                       POST_BUILD
                       COMMAND /usr/bin/libtool -static -o ${FULL_OUTPUT_PATH} ${merge_libs})
  endif()
endfunction(combine_archives)
