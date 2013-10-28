cmake_minimum_required(VERSION 2.8)
project(cam_IC_game_complete)
SET(COMPILE_DEFINITIONS -Werror)

INCLUDE_DIRECTORIES(/opt/vc/include)
INCLUDE_DIRECTORIES(/opt/vc/include/interface)
INCLUDE_DIRECTORIES(/opt/vc/include/interface/vcos)
INCLUDE_DIRECTORIES(/opt/vc/include/interface/vcos/pthreads)
INCLUDE_DIRECTORIES(/opt/vc/include/interface/vmcs_host/linux)
INCLUDE_DIRECTORIES(.)

SET(CMAKE_C_FLAGS "-o3 -w -DRPI_NO_X")
SET(CMAKE_CXX_FLAGS "-o3 -w -DRPI_NO_X")
SET(CMAKE_C_LINK_FLAGS "-pg")

add_executable(cam_IC_game src/RaspiCamControl.c src/RaspiCLI.c src/RaspiPreview.c src/esShader.c src/esTransform.c src/esShapes.c src/esUtil.c src/cam_IC_game.cpp )
target_link_libraries(cam_IC_game /opt/vc/lib/libmmal_core.so /opt/vc/lib/libmmal_util.so /opt/vc/lib/libmmal_vc_client.so /opt/vc/lib/libvcos.so /opt/vc/lib/libbcm_host.so /opt/vc/lib/libEGL.so /opt/vc/lib/libGLESv2.so)
