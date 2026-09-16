cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Keep the factory Recovery entry gesture in bootloader
idf_build_set_property(
    BOOTLOADER_EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/bootloader_components/"
    APPEND)

project(AI-Passport-Baye)
