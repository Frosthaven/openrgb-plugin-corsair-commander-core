#----------------------------------------------------------------------
# OpenRGB Plugin: Corsair iCUE H150i Elite CAPELLIX XT
#
# Build:
#   export OPENRGB_DIR=/path/to/OpenRGB
#   qmake CorsairCapellixXT.pro
#   make -j$(nproc)
#----------------------------------------------------------------------

QT      -= gui
QT      += svg
TEMPLATE = lib
CONFIG  += plugin c++17

TARGET   = OpenRGBCorsairCapellixXTPlugin

# Version info embedded in the plugin binary
VERSION_STRING = "0.1.0"
GIT_COMMIT_ID  = $$system(git rev-parse --short=8 HEAD)
DEFINES += VERSION_STRING=\\\"$$VERSION_STRING\\\"
DEFINES += GIT_COMMIT_ID=\\\"$$GIT_COMMIT_ID\\\"

#----------------------------------------------------------------------
# OpenRGB source tree (set via environment or override here)
#----------------------------------------------------------------------

isEmpty(OPENRGB_DIR) {
    OPENRGB_DIR = $$(OPENRGB_DIR)
}

isEmpty(OPENRGB_DIR) {
    error("OPENRGB_DIR is not set. Point it at your OpenRGB source checkout.")
}

#----------------------------------------------------------------------
# Include paths — OpenRGB headers and all internal subdirectories
#----------------------------------------------------------------------

INCLUDEPATH += $$OPENRGB_DIR

# On Unix, dynamically discover directories containing headers.
# On Windows, $$system() uses cmd.exe where GNU find is unavailable,
# so we list the required OpenRGB subdirectories explicitly.
unix {
    INCLUDEPATH += $$system(find $$OPENRGB_DIR -name \\*.h -exec dirname {} + | sort -u 2>/dev/null)
}

win32 {
    # Mirrors the INCLUDEPATH from OpenRGB.pro
    INCLUDEPATH += \
        $$OPENRGB_DIR/dependencies/ColorWheel               \
        $$OPENRGB_DIR/dependencies/CRCpp                    \
        $$OPENRGB_DIR/dependencies/display-library/include   \
        $$OPENRGB_DIR/dependencies/hidapi-win/include        \
        $$OPENRGB_DIR/dependencies/httplib                   \
        $$OPENRGB_DIR/dependencies/hueplusplus-1.2.0/include \
        $$OPENRGB_DIR/dependencies/hueplusplus-1.2.0/include/hueplusplus \
        $$OPENRGB_DIR/dependencies/json                      \
        $$OPENRGB_DIR/dependencies/libusb-1.0.27/include     \
        $$OPENRGB_DIR/dependencies/mbedtls-3.2.1/include     \
        $$OPENRGB_DIR/dependencies/mdns                      \
        $$OPENRGB_DIR/dependencies/NVFC                      \
        $$OPENRGB_DIR/dependencies/PawnIO                    \
        $$OPENRGB_DIR/dependencies/stb                       \
        $$OPENRGB_DIR/AutoStart                              \
        $$OPENRGB_DIR/dmiinfo                                \
        $$OPENRGB_DIR/hidapi_wrapper                         \
        $$OPENRGB_DIR/i2c_smbus                              \
        $$OPENRGB_DIR/i2c_smbus/Windows                      \
        $$OPENRGB_DIR/i2c_tools                              \
        $$OPENRGB_DIR/interop                                \
        $$OPENRGB_DIR/KeyboardLayoutManager                  \
        $$OPENRGB_DIR/net_port                               \
        $$OPENRGB_DIR/pci_ids                                \
        $$OPENRGB_DIR/qt                                     \
        $$OPENRGB_DIR/RGBController                          \
        $$OPENRGB_DIR/scsiapi                                \
        $$OPENRGB_DIR/serial_port                            \
        $$OPENRGB_DIR/SPDAccessor                            \
        $$OPENRGB_DIR/super_io                               \
        $$OPENRGB_DIR/SuspendResume                          \
        $$OPENRGB_DIR/wmi
}

#----------------------------------------------------------------------
# hidapi link flags
#----------------------------------------------------------------------

unix:!macx {
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi-hidraw
}

macx {
    LIBS += -framework IOKit -framework CoreFoundation
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi
    # <filesystem> requires macOS 10.15+; Qt 5 defaults to 10.13
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    QMAKE_CXXFLAGS += -std=c++17
}

win32 {
    CONFIG  += link_pkgconfig
    PKGCONFIG += hidapi
}

#----------------------------------------------------------------------
# Plugin sources
#----------------------------------------------------------------------

HEADERS += \
    src/CorsairCapellixXTPlugin.h           \
    src/CorsairCapellixXTController.h       \
    src/RGBController_CorsairCapellixXT.h   \
    src/CorsairCapellixXTDetect.h

SOURCES += \
    src/CorsairCapellixXTPlugin.cpp         \
    src/CorsairCapellixXTController.cpp     \
    src/RGBController_CorsairCapellixXT.cpp \
    src/CorsairCapellixXTDetect.cpp

RESOURCES += \
    resources/resources.qrc

#----------------------------------------------------------------------
# Install target
#----------------------------------------------------------------------

unix {
    target.path = $$[QT_INSTALL_PLUGINS]/OpenRGB
    INSTALLS += target
}
