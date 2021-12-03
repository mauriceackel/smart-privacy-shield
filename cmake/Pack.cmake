# Configure packing

set(CPACK_PROJECT_NAME "Smart Privacy Shield")
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Maurice Ackel")
set(CPACK_PACKAGE_CONTACT "Maurice Ackel")

if(APPLE)
    set(CPACK_GENERATOR "Bundle")

    # DMG volume name
    set(CPACK_DMG_VOLUME_NAME "Smart Privacy Shield")

    # App bundle setup
    set(MACOSX_BUNDLE_BUNDLE_NAME "Smart Privacy Shield")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "${PACKAGE}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${SPS_VERSION_MAJOR}.${SPS_VERSION_MINOR}.${SPS_VERSION_PATCH}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${SPS_VERSION_MAJOR}.${SPS_VERSION_MINOR}")

    configure_file(application/data/deploy/mac/Info.plist.in application/Info.plist)
    set(CPACK_BUNDLE_NAME ${MACOSX_BUNDLE_BUNDLE_NAME})
    set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/application/data/deploy/mac/app.icns")
    set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_BINARY_DIR}/application/Info.plist")
    set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/application/data/deploy/mac/startup.sh")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB" "RPM")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")

    # We use the settings below to create the startmenu shortcut with command line args
    # set(CPACK_PACKAGE_EXECUTABLES "smart-privacy-shield;Smart Privacy Shield")

    # An icon filename. The name of a *.ico file used as the main icon for the generated install program.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/application/data/deploy/win/app.ico")

    # Extra NSIS commands that will be added to the end of the install Section, after your install tree is available on the target system.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Smart Privacy Shield.lnk\\\" \\\"$INSTDIR\\\\bin\\\\smart-privacy-shield.exe\\\" '--plugins=\\\"$INSTDIR\\\\plugins\\\" --gst-elements=\\\"$INSTDIR\\\\lib\\\\gstreamer\\\"' ")
endif()

include(CPack)
