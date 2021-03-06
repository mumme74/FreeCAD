add_subdirectory(PythonSource)


SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

SET(EditorLibName TextEditor)
INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PYTHON_INCLUDE_DIRS}
)
FILE(GLOB CMakeCache RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/CMakeCache.txt)



IF(CMAKE_BUILD_TYPE)
    add_definitions(-DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
ENDIF(CMAKE_BUILD_TYPE)

if(WIN32)
    add_definitions(-DFCGui) # For GUIExport to work
endif(WIN32)

# qt specific
if (BUILD_QT5)
    include_directories(
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5PrintSupport_INCLUDE_DIRS}
        ${Qt5UiTools_INCLUDE_DIRS}
    )
    list(APPEND Common_LIBS
        ${Qt5Core_LIBRARIES}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5PrintSupport_LIBRARIES}
        ${Qt5UiTools_LIBRARIES}
    )
else()
    include_directories(
        ${QT_INCLUDE_DIR}
    )
    list(APPEND Common_LIBS
        ${QT_LIBRARIES}
        ${QT_QTUITOOLS_LIBRARY}
    )
endif()

# generic syntax highlighter
if (KF5SYNTAX_LIBDIR)
    list(APPEND Common_LIBS ${KF5SYNTAX_LIB})
    add_definitions(-DBUILD_KF5SYNTAX)
    include_directories(APPEND ${KF5SYNTAX_INCLUDE_DIRS})
    link_directories(${KF5SYNTAX_LIBDIR})
endif(KF5SYNTAX_LIBDIR)

# dependencies
list(APPEND Common_LIBS
    FreeCADBase FreeCADApp)

# -------------------------------------------------------------------------------------
# FIXME!!
# should be removed or selectable via a cmake option
SET(BUILD_JEDI 1)
if(BUILD_JEDI)
    SET(JediLibName JediLib)
    SET(Jedi_CPP_SRCS PythonJedi.cpp)
    SET(Jedi_HPP_SRCS PythonJedi.h)
    fc_wrap_cpp(Jedi_MOC_SRCS ${Jedi_HPP_SRCS})
    SET(Jedi_SRCS ${Jedi_CPP_SRCS} ${Jedi_HPP_SRCS} ${Jedi_MOC_SRCS})

    #create Jedi C++ wrapper library
    ADD_LIBRARY(${JediLibName} STATIC ${Jedi_SRCS})
    TARGET_LINK_LIBRARIES(${JediLibName} ${Common_LIBS})
    add_definitions(-DBUILD_JEDI)
endif(BUILD_JEDI)

# -------------------------------------------------------------------------------------


# -------------------------------------------------------------------------------------

# The editor sources
SET(TextEditor_CPP_SRCS
    EditorView.cpp
    LangPluginBase.cpp
    PythonCode.cpp
    PythonEditor.cpp
    PythonLangPlugin.cpp
    PythonSyntaxHighlighter.cpp
    SyntaxHighlighter.cpp
    TextDocumentEditorView.cpp
    TextEditor.cpp
    TextEditBlockData.cpp
)
SET(TextEditor_HPP_SRCS
    EditorView.h
    LangPluginBase.h
    PythonCode.h
    PythonEditor.h
    PythonLangPlugin.h
    PythonSyntaxHighlighter.h
    SyntaxHighlighter.h
    TextDocumentEditorView.h
    TextEditor.h
    TextEditBlockData.h
)
SET(TextEditor_MOC_HDRS
    EditorView.h
    LangPluginBase.h
    PythonCode.h
    PythonEditor.h
    PythonLangPlugin.h
    PythonSyntaxHighlighter.h
    SyntaxHighlighter.h
    TextDocumentEditorView.h
    TextEditor.h
    TextEditBlockData.h
)
# run moc
fc_wrap_cpp(TextEditor_MOC_SRCS ${TextEditor_MOC_HDRS})

SET(TextEditor_SRCS
    ${TextEditor_CPP_SRCS}
    ${TextEditor_HPP_SRCS}
    ${TextEditor_MOC_HDRS}
)

# for debugging/dumping syntax parser and AST code
if(BUILD_PYTHON_DEBUGTOOLS)
    SET(TextEditor_DbgTools_CPP_SRCS
        PythonCodeDebugTools.cpp)
    SET(TextEditor_DbgTools_HPP_SRCS
        PythonCodeDebugTools.h)
    SET(TextEditor_DbgTools_MOC_HDRS
        PythonCodeDebugTools.h)
    add_definitions(-DBUILD_PYTHON_DEBUGTOOLS=1)
    # run moc
    fc_wrap_cpp(TextEditor_DbgTools_MOC_SRCS ${TextEditor_DbgTools_MOC_HDRS})

    SET(TextEditor_DbgTools_SRCS
        ${TextEditor_DbgTools_CPP_SRCS}
        ${TextEditor_DbgTools_HPP_SRCS}
        ${TextEditor_DbgTools_MOC_HDRS})

endif()
SOURCE_GROUP("TextEditor" FILES ${TextEditor_SRCS})

# resources
SET(TextEditor_RES_SRCS
    #Icons/resource.qrc
    #Language/translation.qrc
)
if (BUILD_QT5)
    QT5_ADD_RESOURCES(TextEditor_QRC_SRCS ${TextEditor_RES_SRCS})
else(BUILD_QT5)
    QT4_ADD_RESOURCES(TextEditor_QRC_SRCS ${TextEditor_RES_SRCS})
endif(BUILD_QT5)
SET(TextEditor_Dlg_SRCS
    ${TextEditor_Dlg_CPP_SRCS}
    ${TextEditor_Dlg_HPP_SRCS}
    ${TextEditor_Dlg_MOC_SRCS}
)


# wrap it all up for lib TextEditor
SET(TextEditorAll_SRCS ${TextEditor_SRCS}
                       ${TextEditor_Dlg_SRCS}
                       ${TextEditor_QRC_SRCS}
                       ${TextEditor_DbgTools_SRCS}
)
SOURCE_GROUP("Editor" FILES ${TextEditorAll_SRCS})


# precompiled?
if (BUILD_USE_PCH)
    add_definitions(-D_PreComp_)
    ADD_MSVC_PRECOMPILED_HEADER(${EditorLibName} PreCompiled.h PreCompiled.cpp TextEditor_CPP_SRCS)
endif(BUILD_USE_PCH)

#create library
ADD_LIBRARY(${EditorLibName} STATIC ${TextEditorAll_SRCS})
TARGET_LINK_LIBRARIES(${EditorLibName} ${Common_LIBS} ${JediLibName} PythonSource)


# install to default target defined by
INSTALL(TARGETS ${EditorLibName}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# -------------------------------------------------------------------------------


