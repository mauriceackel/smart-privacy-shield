# Install plugin

set(CURRENT_PLUGIN_BASE_DIR "${PLUGIN_DIR}/spsbase")
set(CURRENT_PLUGIN_GST_ELEMENTS_DIR "${CURRENT_PLUGIN_BASE_DIR}/gst-plugins")
install(CODE "set(CURRENT_PLUGIN_BASE_DIR \"\${PLUGIN_DIR}/spsbase\")")
install(CODE "set(CURRENT_PLUGIN_GST_ELEMENTS_DIR \"\${CURRENT_PLUGIN_BASE_DIR}/gst-plugins\")")

# Add main plugin lib
install(TARGETS spsbase
  RUNTIME DESTINATION ${CURRENT_PLUGIN_BASE_DIR} # Copy everything to the plugin base dir
  LIBRARY DESTINATION ${CURRENT_PLUGIN_BASE_DIR}
  RESOURCE DESTINATION ${CURRENT_PLUGIN_BASE_DIR} # Copy gsettings schema etc.
)

# Add required stock gst elements
find_package(GStreamer REQUIRED app videoconvert videoscale)
set(GST_STOCK_ELEMENT_FILES ${GSTREAMER_APP_LIBRARIES} ${GSTREAMER_VIDEOCONVERT_LIBRARIES} ${GSTREAMER_VIDEOSCALE_LIBRARIES})

foreach(FILE ${GST_STOCK_ELEMENT_FILES})
    get_filename_component(RESOLVED_FILE "${FILE}" REALPATH)
    list(APPEND RESOLVED_GST_STOCK_ELEMENT_FILES "${RESOLVED_FILE}")
endforeach()
install(FILES ${RESOLVED_GST_STOCK_ELEMENT_FILES} DESTINATION ${CURRENT_PLUGIN_GST_ELEMENTS_DIR})

if(UNIX AND NOT APPLE)
  set_target_properties(gstregions gstchangedetector gstobjdetection gstobstruct gstwinanalyzer
    PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
  )
endif()

# Add required custom gst elements
install(TARGETS gstregions gstchangedetector gstobjdetection gstobstruct gstwinanalyzer
  RUNTIME DESTINATION ${CURRENT_PLUGIN_GST_ELEMENTS_DIR}
  LIBRARY DESTINATION ${CURRENT_PLUGIN_GST_ELEMENTS_DIR}
)

# Install detection model
if(APPLE)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/models/mac.onnx" DESTINATION "${CURRENT_PLUGIN_BASE_DIR}/${DATA_DIR}/models")
elseif(UNIX)
  # TODO: Add back once there is a linux model 
  # install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/models/linux.onnx" DESTINATION "${CURRENT_PLUGIN_BASE_DIR}/${DATA_DIR}/models")
elseif(WIN32)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/models/windows.onnx" DESTINATION "${CURRENT_PLUGIN_BASE_DIR}/${DATA_DIR}/models")
endif()

# Check main install file for runtime dependency install
