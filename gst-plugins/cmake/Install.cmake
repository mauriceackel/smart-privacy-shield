# Install gstreamer elements used by application directly

# ----------- Installation of essential gstreamer elements
set(GST_ELEMENTS_DIR "${LIB_DIR}/gstreamer")
install(CODE "set(GST_ELEMENTS_DIR \"\${LIB_DIR}/gstreamer\")")

# Install stock gstreamer elements
find_package(GStreamer REQUIRED coreelements compositor playback encoding videoconvert gtk)
set(GST_STOCK_ELEMENT_FILES ${GSTREAMER_COREELEMENTS_LIBRARIES} ${GSTREAMER_COMPOSITOR_LIBRARIES} ${GSTREAMER_PLAYBACK_LIBRARIES} ${GSTREAMER_ENCODING_LIBRARIES} ${GSTREAMER_VIDEOCONVERT_LIBRARIES} ${GSTREAMER_GTK_LIBRARIES})
# Resolve symlinks
foreach(FILE ${GST_STOCK_ELEMENT_FILES})
    get_filename_component(RESOLVED_FILE "${FILE}" REALPATH)
    list(APPEND RESOLVED_GST_STOCK_ELEMENT_FILES "${RESOLVED_FILE}")
endforeach()
install(FILES ${RESOLVED_GST_STOCK_ELEMENT_FILES} DESTINATION ${GST_ELEMENTS_DIR})

# Install custom gstreamer elements
install(TARGETS gstautocompositor gstscreencapsrc
  RUNTIME DESTINATION ${GST_ELEMENTS_DIR} # Ensure dlls are placed here as well on windows
  LIBRARY DESTINATION ${GST_ELEMENTS_DIR}
)
# Add common gst element library to lib folder
install(TARGETS gstspscommon
  LIBRARY DESTINATION ${LIB_DIR}
)
