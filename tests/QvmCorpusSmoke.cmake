if(NOT DEFINED Q3VM OR NOT EXISTS "${Q3VM}")
    message(FATAL_ERROR "Q3VM executable was not supplied")
endif()
if(NOT DEFINED QVM_CORPUS_DIR OR NOT IS_DIRECTORY "${QVM_CORPUS_DIR}")
    message(FATAL_ERROR "QVM_CORPUS_DIR must name a directory containing .qvm files")
endif()
if(NOT DEFINED QVM_CORPUS_OUTPUT_DIR)
    set(QVM_CORPUS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/qvm-corpus-smoke")
endif()

file(GLOB qvm_files "${QVM_CORPUS_DIR}/*.qvm")
if(NOT qvm_files)
    message(FATAL_ERROR "No .qvm files found in ${QVM_CORPUS_DIR}")
endif()
file(MAKE_DIRECTORY "${QVM_CORPUS_OUTPUT_DIR}")

foreach(qvm_file IN LISTS qvm_files)
    get_filename_component(module "${qvm_file}" NAME_WE)
    execute_process(COMMAND "${Q3VM}" info "${qvm_file}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "info failed for ${qvm_file}: ${result}")
    endif()

    execute_process(COMMAND "${Q3VM}" disassemble "${qvm_file}"
                            --output "${QVM_CORPUS_OUTPUT_DIR}/${module}.asm"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "disassemble failed for ${qvm_file}: ${result}")
    endif()

    execute_process(COMMAND "${Q3VM}" pseudocode "${qvm_file}"
                            --output "${QVM_CORPUS_OUTPUT_DIR}/${module}.c"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "pseudocode failed for ${qvm_file}: ${result}")
    endif()

    execute_process(COMMAND "${Q3VM}" decompile "${qvm_file}" --module "${module}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "decompile failed for ${qvm_file}: ${result}")
    endif()

    set(export_dir "${QVM_CORPUS_OUTPUT_DIR}/${module}-export")
    execute_process(COMMAND "${Q3VM}" export "${qvm_file}" --module "${module}"
                            --output "${export_dir}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "export failed for ${qvm_file}: ${result}")
    endif()
    if(NOT EXISTS "${export_dir}/${module}.asm" OR NOT EXISTS "${export_dir}/${module}.c")
        message(FATAL_ERROR "export did not write both files for ${qvm_file}")
    endif()

    # A standalone shim cannot initialize the complete game/cgame engine, but
    # it must either run or return its documented structured runtime failure.
    execute_process(COMMAND "${Q3VM}" run "${qvm_file}" --module "${module}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0 AND NOT result EQUAL 2)
        message(FATAL_ERROR "run crashed or failed to load ${qvm_file}: ${result}")
    endif()
endforeach()
