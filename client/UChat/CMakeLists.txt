cmake_minimum_required(VERSION 3.16)

project(UChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
        Widgets
        LinguistTools
        NETWORK
        Svg
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
        Widgets
        LinguistTools
        NETWORK
        Svg
)

# qt feature "timezone"
set(QT_FEATURE_TIMEZONE TRUE)

set(SOURCES
    main.cpp
    logindialog.cpp
    mainwindow.cpp
    ChatManager.cpp
    ChatConnection.cpp
    HttpMgr.cpp
    ContactManager.cpp
    ChatUIManager.cpp
    AddContactDialog.cpp
    timerbutton.cpp
    global.cpp
    MessageDelegate.cpp
    ChatMessageModel.cpp
)

set(HEADERS
    logindialog.hpp
    mainwindow.hpp
    ChatManager.h
    ChatConnection.h
    HttpMgr.h
    ContactManager.h
    ChatUIManager.h
    AddContactDialog.h
    timerbutton.hpp
    Singleton.h
    global.h
    MessageDelegate.h
    ChatMessageModel.h
)

if (${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(UChat
            MANUAL_FINALIZATION
            ${SOURCES}
            ${HEADERS}
            resource.qrc
            logindialog.hpp logindialog.cpp logindialog.ui
    )
    # Define target properties for Android with Qt 6 as:
    #    set_property(TARGET UChat APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
    #                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
    # For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else ()
    if (ANDROID)
        add_library(UChat SHARED
                ${SOURCES}
                ${HEADERS}
        )
        # Define properties for Android with Qt 5 after find_package() calls as:
        #    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else ()
        add_executable(UChat
                ${SOURCES}
                ${HEADERS}
        )
    endif ()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif ()

target_include_directories(UChat PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(UChat PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Svg
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if (${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.UChat)
endif ()
set_target_properties(UChat PROPERTIES
        ${BUNDLE_ID_OPTION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS UChat
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(UChat)
endif ()
