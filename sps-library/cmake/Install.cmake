# Install SPS library

# ----------- Basic installation rules for binary and library
install (TARGETS sps-library
  EXPORT SpsTargets DESTINATION ${LIB_DIR}
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
)

# ----------- Installation of headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sps DESTINATION "${INCLUDE_DIR}" FILES_MATCHING PATTERN "*.h")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sps/spsversion.h DESTINATION "${INCLUDE_DIR}/sps")
