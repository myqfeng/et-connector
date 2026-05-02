cmake_minimum_required(VERSION 3.16)

# 设置CMake策略
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

project(EasyTierConnector VERSION 1.0.0 LANGUAGES CXX)

# 设置默认安装目录为项目根目录的Install文件夹
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/Install" CACHE PATH "Installation directory" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt6包
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg)

# 源文件
set(SOURCES
    src/main.cpp
    src/SystemTray.cpp
    src/SettingsDialog.cpp
    src/AboutDialog.cpp
    src/QuitConfirmDialog.cpp
    src/ETRunService.cpp
    src/ConfigManager.cpp
    src/CasdoorLogin.cpp
)

# 头文件
set(HEADERS
    src/SystemTray.h
    src/SettingsDialog.h
    src/AboutDialog.h
    src/QuitConfirmDialog.h
    src/ETRunService.h
    src/ConfigManager.h
    src/CasdoorLogin.h
)

# 资源文件
set(RESOURCES
    resources.qrc
)

# 创建可执行文件
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
)

# Windows特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # 生成 RC 文件引入 manifest 和图标
    set(APP_MANIFEST "${CMAKE_SOURCE_DIR}/app.manifest")
    set(APP_ICON "${CMAKE_SOURCE_DIR}/assets/favicon.ico")
    set(APP_RC "${CMAKE_BINARY_DIR}/app.rc")

    # 检查图标文件是否存在
    if(NOT EXISTS ${APP_ICON})
        message(WARNING "图标文件 ${APP_ICON} 不存在，请将 favicon.svg 转换为 favicon.ico 格式")
    endif()

    # 自动生成 RC 文件
    file(WRITE ${APP_RC}
"// 自动生成的资源文件
#include <windows.h>

// 应用程序图标
IDI_ICON1   ICON    \"${APP_ICON}\"

// 应用程序清单
1   RT_MANIFEST   \"${APP_MANIFEST}\"
")

    # 将 RC 文件添加到源文件列表
    target_sources(${PROJECT_NAME} PRIVATE ${APP_RC})
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# 复制etcore文件夹到输出目录和安装目录
if(WIN32)
    set(ETCORE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/etcore/windows")
else()
    set(ETCORE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/etcore/linux")
endif()

# 添加自定义命令：在构建时复制etcore文件夹到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${ETCORE_SOURCE_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/etcore
    COMMENT "复制etcore文件夹到输出目录"
)

# 安装etcore目录
install(CODE "
    message(STATUS \"部署etcore...\")
    # 获取安装目录的绝对路径
    get_filename_component(INSTALL_ABS_PATH \"\${CMAKE_INSTALL_PREFIX}\" ABSOLUTE)
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\" -E copy_directory
            \"${ETCORE_SOURCE_DIR}\"
            \"\${INSTALL_ABS_PATH}/bin/etcore\"
    )
")

# 安装时使用windeployqt部署Qt运行时
if(WIN32)
    # 定义安装目录变量
    set(CMAKE_INSTALL_BINDIR "bin")
    
    # 直接调用windeployqt，最小化部署Qt运行时
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${QT_HOST_PATH}/bin ${QT6_HOST_PATH}/bin ${QT5_HOST_PATH}/bin)
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"使用windeployqt部署Qt运行时...\")
            # 获取安装目录的绝对路径
            get_filename_component(INSTALL_ABS_PATH \"\${CMAKE_INSTALL_PREFIX}\" ABSOLUTE)
            set(EXE_PATH \"\${INSTALL_ABS_PATH}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}.exe\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --no-quick
                    --no-system-d3d-compiler
                    --no-translations
                    --no-opengl-sw
                    \"\${EXE_PATH}\"
                WORKING_DIRECTORY \"\${INSTALL_ABS_PATH}/${CMAKE_INSTALL_BINDIR}\"
                RESULT_VARIABLE deploy_result
            )
            if(NOT deploy_result EQUAL 0)
                message(WARNING \"windeployqt执行失败，返回码: \${deploy_result}\")
            endif()
        ")
    else()
        message(WARNING "未找到windeployqt工具，跳过Qt运行时部署")
    endif()
endif()
