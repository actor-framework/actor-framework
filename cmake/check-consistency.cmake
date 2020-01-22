execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
                "${file_under_test}" "${generated_file}"
                RESULT_VARIABLE result)
if(result EQUAL 0)
  # files still in sync
else()
  message(SEND_ERROR "${file_under_test} is out of sync! Run target "
                     "'update-enum-strings' to update automatically")
endif()
