This is the first version of Raspberry Pi Camera 2D controller

download to a raspberry pi equipped with raspi camera.Open a terminal under the directory. Type 'cmake .', then followed with 'make'to compile
run the software with command './cam_IC_game'. Now you can move the camera to control the triangle to chase the running box.

GPU: for running on raspberry pi include directories: 	"/opt/vc/include"
						      	"/opt/vc/include/interface"
						      	"/opt/vc/include/interface/vcos"
							"/opt/vc/include/interface/vcos/pthreads"
							"/opt/vc/include/interface/vmcs_host/linux"
                                  include libraries:	/opt/vc/lib/libmmal_core.so 
							/opt/vc/lib/libmmal_util.so 
							/opt/vc/lib/libmmal_vc_client.so 
							/opt/vc/lib/libvcos.so 
							/opt/vc/lib/libbcm_host.so 
							/opt/vc/lib/libEGL.so 
							/opt/vc/lib/libGLESv2.so

Reference:

[1] matrix.h:  Jos de Jong,A simple matrix class, c++ code, Nov 2007. Updated March 2010,edit by Jiajing Sun
[2] matrixInverse.h: written by Mike Dinolfo, 12,1998.
[3] esShader.c, esShapes.c, esTransform.c, esUtil.c, esUtil.h: Aaftab Munshi, Dan Ginsburg, Dave Shreiner,OpenGL(R) ES 2.0 Programming Guide.
[4] RaspiCamControl.c, RaspiCamControl.h, RaspiCLI.c, RaspiCLI.h, RaspiPreview.c, RaspiPreview.h, RaspiStillYUV.c, RaspiVid.c:James Hughes,Broadcom Europe Ltd




