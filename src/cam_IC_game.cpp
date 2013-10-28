//
// Authors:   Jiajing Sun
// Project: image alignment on Raspberry Pi

// cam_IC_gl.cpp
//
// Reference: Aaftab Munshi, Dan Ginsburg, Dave Shreiner ,OpenGL(R) ES 2.0 Programming Guide

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

//new
#include <stdlib.h>
#include "esUtil.h"
#include "time.h"

extern "C"{
#include <GLES2/gl2ext.h>

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
}

#include <semaphore.h>
#include "matrixInverse.h"
#include "matrix.h"
/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

// Max bitrate we allow for recording
const int MAX_BITRATE = 30000000; // 30Mbits/s
using namespace std;

//new 
int nCount=0;
		
int mmal_status_to_int(MMAL_STATUS_T status);

/** Structure containing all state information for the current run
 */
typedef struct
{
   int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
   int width;                          /// Requested width of image
   int height;                         /// requested height of image
   int bitrate;                        /// Requested bitrate
   int framerate;                      /// Requested frame rate (fps)
   int graymode;			/// capture in gray only (2x faster)
   int immutableInput;      /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
                                       /// the camera output or the encoder output (with compression artifacts)
   RASPIPREVIEW_PARAMETERS preview_parameters;   /// Preview setup parameters
   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
   MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
   MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
   MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

   MMAL_POOL_T *video_pool; /// Pointer to the pool of buffers used by encoder output port
   ESContext *esContext;
   
} RASPIVID_STATE;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct
{
   FILE *file_handle;                   /// File handle to write buffer data to.
   VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
   RASPIVID_STATE *pstate;            /// pointer to our state in case required in callback
} PORT_USERDATA;


typedef struct
{
   // Handle to a program object
   GLuint programObject[6];
   
   // Handle to frame buffer
   GLuint frameBuffer[6];

   // Attribute locations
   GLint  positionLoc[6];
   GLint  texCoordLoc[4];

   // Sampler location
   GLint samplerLoc[6];

   // Texture handle
   GLuint textureId[8];
   
   //other parameter for implementation
   GLint warp_pLoc[2];
   GLint grad_fLoc1;
   GLint grad_fLoc2;
   GLint image_sLoc[2];
   GLint flagLoc;
   GLint colorLoc;
   
   GLfloat shift[2];// = {0.0, 0.0};                  
   
	GLfloat color[4];// = {0.0, 1.0, 0.0, 1.0}; 
   
	float totaltime;
	float gap ;//= 2.0
   
	int change_color;// = 0
 
  
} UserData;


typedef struct
{	
	int w;
	int h;
   // Texture data
	char* imageData;
	char* tmpltData;
	float* warp_p;
} TextureImg;

TextureImg texImg;
//GLuint textureId;

int flag = 0;



EGLBoolean esLoadTextFile (char *filename, char *&textfile, int &length)
{
    FILE* f;
    size_t elementsRead;

    /*if (!filename)// || !textfile
    {
        
        printf("no filename/n");
        return EGL_FALSE;
    }*/

    length = 0;

    f = fopen(filename, "r");

    if (!f)
    {
        printf("no filen/n");
        return EGL_FALSE;
    }

    if(fseek(f, 0, SEEK_END))
    {
        fclose(f);
		printf("no filen/n");
        return EGL_FALSE;
    }

    //textfile->length = ftell(f);
	length = ftell(f);

    if (length < 0 )
    {
        fclose(f);

        length = 0;

        return EGL_FALSE;
    }
	
    textfile = (char*) malloc((size_t)length + 1);

    memset(textfile, 0, (size_t)length + 1);

    rewind(f);

    elementsRead = fread(textfile, 1, (size_t)length, f);

    fclose(f);

    return EGL_TRUE;
}




void MatrixMultiply(float* MatrixA, double* MatrixB, double* MatrixC, int ARow, int AColumn, int BColumn)
{
	//Name:		MatrixMultiply
	//			returns multiplication result of Matrix A and Matrix B by Matrix C
	//Params:	MatrixA & MatrixB: Multipiler in 1D array
	//			MatrixC : result matrix in 1D array
	//			ARow: row number for MatrixA
	//			AColumn: column number for MatrixA
	//			BColumn: column number for MatrixB
	int i;
	int j;
	int k;

	for (i = 0; i != ARow; i ++)
	{
		for (j = 0; j != BColumn; j ++)
		{
			MatrixC[i*BColumn+j] = 0;
			for (k = 0; k !=AColumn; k ++)
			{
				MatrixC[i*BColumn+j] += (double)MatrixA[i*AColumn+k] * MatrixB[k*BColumn+j];
			}
		}
	}
}



void MatrixCopy(double* MatrixA, float* MatrixB, int length)
{
	//Name:		MatrixCopy
	//			Copies double matrix A to a float matrix B
	//Params:	MatrixA: Original double floating point matrix in 1D array
	//			MatrixB : result floating point matrix in 1D array
	//			length: total size of the Matrices
	int i;
	for (i = 0; i != length; i ++)
	{
		MatrixB[i] = (float)MatrixA[i];
	}
}



double* hessian(char** Hs, int length ){
	
	//Name:		hessian
	//			Computes the Hessian matrix in the algorithm
	//Params:	Hs: Gained multiplied matrix from GPU processing
	//			length: total size of the Matrices
	
	double h[2][2] = {{0.0, 0.0},{0.0, 0.0}};
	double sum[6] = {0.0,0.0,0.0,0.0,0.0,0.0};
	double* hinv = new double[4];
	Matrix H = Matrix(2,2);
	int index;
	// printf("Hessian\n");
	for(int i = 0; i < length; i ++)
	{
		index = 4*i;
		sum[0] += Hs[0][index];
		sum[1] += Hs[0][index+1];
		sum[2] += Hs[0][index+2];
		sum[3] += Hs[0][index+3];
		sum[4] += Hs[1][index];
		sum[5] += Hs[1][index+1];
		
	}
	h[0][0] = (sum[0] * 256 + sum[1] - 32896.0 * length)*64;
	h[0][1] = (sum[2] * 256 + sum[3] - 32896.0 * length)*64;
	h[1][1] = (sum[4] * 256 + sum[5] - 32896.0 * length)*64;
	H = Diag(2);
	hinv = H.getAll();

	 H(1,1) = h[0][0];
	 H(1,2) = h[0][1];
	 H(2,1) = h[0][1];
	 H(2,2) = h[1][1];
	 H = Inv(H);
     hinv = H.getAll();
     return hinv;
 }



void sd_update(float* warp_p, double* hinv, char* sd_delta, int length){
	
	//Name:		sd_update
	//			Computes the rest part for warping parameter update
	//Params:	Hs: Gained multiplied matrix from GPU processing
	//			length: total size of the Matrices
	
	float* sd_delta_p = new float[2];
	double sum[4] = {0.0, 0.0, 0.0, 0.0};
	double* dp_xy = new double[2];
	double* dp;// = new double[9];
	double* warp_new = new double[9];
	//double* warp_new2 = new double[9];
	int index;
	Matrix delta_p = Diag(3); 
	
	 for(int i = 0; i < length; i ++)
	 {
		index = 4*i;
		sum[0] += sd_delta[index];
		sum[1] += sd_delta[index+1];
		sum[2] += sd_delta[index+2];
		sum[3] += sd_delta[index+3];
	 }
	 sd_delta_p[0] = (float)(sum[0]*256 + sum[1] - 32896.0 * length)* 64;
	 sd_delta_p[1] = (float)(sum[2]*256 + sum[3] - 32896.0 * length)* 64;
     //printf("%f  %f\n", sd_delta_p[0], sd_delta_p[1]);
	 MatrixMultiply(sd_delta_p, hinv, dp_xy, 1, 2, 2);
	 delta_p(1,3) = dp_xy[0];
	 delta_p(2,3) = dp_xy[1];
	 delta_p = Inv(delta_p);
	 dp = delta_p.getAll();
	 
	 warp_p[0] = warp_p[0] +1;
	 warp_p[4] = warp_p[4] +1;
	 MatrixMultiply(warp_p, dp, warp_new, 3, 3, 3);
	 MatrixCopy(warp_new, warp_p, 9);
	 warp_p[0] = 1;
	 warp_p[4] = 1;

	 delete [] warp_new;	 
	 //delete [] dp;
	 delete [] sd_delta_p;
}



///
// Create a simple 2x2 texture image with four different colors
//
void CreateSimpleTexture2D(ESContext *esContext, GLuint textureId)
{
    //Name:		CreateSimpleTexture2D
	//			Create a simple 2x2 texture image with four different colors
	//Params:	esContext: ESContext struct containing OpenGL es Context parameters
	//			textureId: texture ID for the created texture
   memset(texImg.imageData, 0,texImg.w*texImg.h);

   //GLubyte* pixels = (GLubyte*) texImg.imageData;
   // Use tightly packed data
   glPixelStorei ( GL_UNPACK_ALIGNMENT, 1 );

   // Generate a texture object
   //glGenTextures ( 1, &textureId );

   // Bind the texture No.1 object
   glBindTexture ( GL_TEXTURE_2D, textureId );

   // Load the texture
   glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, esContext->width, esContext->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, texImg.imageData );

   // Set the filtering mode
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
   glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

   
   //return textureId;
}




void textureUpdate(ESContext *esContext, float deltaTime )
{
	//Name:		textureUpdate
	//			update texture content for each iteration
	//Params:	esContext: ESContext struct containing OpenGL es Context parameters
	//			deltaTime: time lapse from last update
	int move = 0;
	UserData *userData = (UserData *)esContext->userData;
	//userData ->time += deltaTime;
	//memset(texImg.imageData,0,texImg.w*texImg.h);
	GLubyte* ImagePixels = (GLubyte*) texImg.imageData;
	glBindTexture ( GL_TEXTURE_2D, userData->textureId[7] );
	// Load the texture
	glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, texImg.w, texImg.h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, ImagePixels );
	
	GLubyte* TmpltPixels = (GLubyte*) texImg.tmpltData;
	glBindTexture ( GL_TEXTURE_2D, userData->textureId[6] );
	// Load the texture
	glTexImage2D ( GL_TEXTURE_2D, 0, GL_LUMINANCE, texImg.w, texImg.h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, TmpltPixels );
	// Set the filtering mode
	
	userData ->totaltime = userData ->totaltime + deltaTime;
	//cout<<userData->time<<endl;
	if(userData ->change_color == 1)
	{
		userData ->color[0] = ( (float)(rand() % 15000) / 20000.0f ) + 0.25f;
        userData ->color[1] = ( (float)(rand() % 15000) / 20000.0f ) + 0.25f;
        userData ->color[2] = ( (float)(rand() % 15000) / 20000.0f ) + 0.25f;
        userData ->color[3] = 1.0;
        userData ->change_color = 0;
        userData ->totaltime = 0.0;
        userData ->gap = userData ->gap * 0.9;
        move = 1;
		//userData->time = 0.0;  
	}else if(userData ->totaltime >= userData ->gap )
	{
		
		move = 1;
		userData ->totaltime = 0.0;
	    //userData->change_color = 0;
	}
	
	
	if(move == 1)
	{
		userData ->shift[0] = ( (float)(rand() % 9500) / 5000.0f ) - 1.0f;
		userData ->shift[1] = ( (float)(rand() % 9500) / 5000.0f ) - 1.0f;
		move =0;
		//cout<<shift[0]<<endl;
	}
	
}


//
// Initialize target program object
//
GLuint InitProgram (GLuint program)
{
	//Name:		InitProgram
	//			Initialize different GPU program for given program ID
	//Params:	program: Program ID

	GLuint prog;
	char *shaderName = new char[80];
	char number[4];
	sprintf(number, "%d", (int)program);
	GLbyte *vShaderStr;
	
	GLbyte *fShaderStr;
	int vlength = 0, flength = 0;
	char *vertName = new char[80];
	if(program != 4 && program != 5)
	{
		strcpy(vertName, "Shader/fulltexture.vert.glsl");
	}
	else if(program == 4){
		strcpy(vertName, "Shader/triangle.vert.glsl");
	}else if(program == 5){
		strcpy(vertName, "Shader/square.vert.glsl");
	}


	if(esLoadTextFile(vertName, (char * &)vShaderStr,vlength) == EGL_FALSE)
	{
		printf("Failed to load vertex shader!\n");
		exit(0);
	}
	
	strcpy(shaderName, "Shader/program");
	strcat(shaderName, number);
	strcat(shaderName, ".frag.glsl");

	if(esLoadTextFile(shaderName, (char * &)fShaderStr,flength) == EGL_FALSE)
	{
		printf("Failed to load fragment shader!\n");
		exit(0);
	}

	if( vlength != 0 && flength != 0 )
	{
		prog = esLoadProgram ( (const char *)vShaderStr, (const char *)fShaderStr );
		//printf("return %d\n", (int)prog);
	}
	else
	{
		printf("Failed to load fragment shader!\n");
		exit(0);
	}
	//printf("program initiated");
	//printf("return %d\n", (int)program);
	return prog;
}

//
// Initialize target program object
//
GLuint InitFrameBuffer (GLuint texture, GLint texWidth, GLint texHeight)
{
	//Name:		InitFrameBuffer
	//			Initiate frame buffer for containing result from a GPU program
	//Params:	texture: Texture ID for the texture that bind with 
	//			deltaTime: time lapse from last update
	
	///////////////////////////////////////////////////////////////////////////////////////
	// Initialize Frame Buffer Object                                                    //  
	// And the texture that rendered to, and its width and height                        //
	// Param: texture ID of a exist texture                                              //
	///////////////////////////////////////////////////////////////////////////////////////
   	//GLint  texWidth = 640, texHeight = 480;
   	GLuint framebuffer;
	glGenFramebuffers(1, &framebuffer);
	//glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glGenerateMipmap(GL_TEXTURE_2D);
	
	return framebuffer;
   
}

///
// Initialize program objects and frame buffer objects
//
int Init ( ESContext *esContext )
{
   	esContext->userData = malloc(sizeof(UserData));	
   	UserData *userData = (UserData *)esContext->userData;
   	GLuint texture[8];
   	srand(time(NULL));
   	userData ->totaltime = 0.0;
   	userData ->change_color = 0;
   	userData ->gap = 10.0;
   	// Generate necessary textures
   	glGenTextures(8, texture);
   	//std::cout<<sizeof(UserData)<<std::endl;
   	int width  =  esContext->width/2;
   	int height =  esContext->height/2;
	
	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer for GRADIENT (SHADER)                          //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
   	userData->programObject[0] = (GLuint)InitProgram ( 0 );  

   	// Get the attribute locations
   	userData->positionLoc[0] = glGetAttribLocation ( userData->programObject[0], "a_position" );
   	userData->texCoordLoc[0] = glGetAttribLocation ( userData->programObject[0], "a_texCoord" );
   
   	// Get the sampler location
    userData->samplerLoc[0] = glGetUniformLocation ( userData->programObject[0], "s_texture" );
    userData->grad_fLoc1    = glGetUniformLocation ( userData->programObject[0], "g_filter1"  );
    userData->grad_fLoc2    = glGetUniformLocation ( userData->programObject[0], "g_filter2"  );
    userData->image_sLoc[0]    = glGetUniformLocation ( userData->programObject[0], "i_size"  );

	// Load the img as texture
	//LoadTexture ( "image0_0.jpg", texture[5]);
	CreateSimpleTexture2D( esContext,texture[6] );
	
	// Initiate framebuffer to contain X and Y for both Img_warped and Tmp
	userData->frameBuffer[0] = InitFrameBuffer ( texture[0], width, height );
   	
   	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer Object for Warping (SHADER)                    //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
	//printf("return %d\n", (int)prog);
	userData->programObject[1] = InitProgram ( 1 );
   	// Get the attribute locations
   	userData->positionLoc[1] = glGetAttribLocation ( userData->programObject[1], "a_position" );
   	userData->texCoordLoc[1] = glGetAttribLocation ( userData->programObject[1], "a_texCoord" );
   
   	// Get the sampler location
   	userData->samplerLoc[1] = glGetUniformLocation ( userData->programObject[1], "img_texture" );
   	userData->samplerLoc[2] = glGetUniformLocation ( userData->programObject[1], "tmplt_texture" );
   	userData->samplerLoc[3] = glGetUniformLocation ( userData->programObject[1], "g_texture" );
   	userData->warp_pLoc[0]  = glGetUniformLocation ( userData->programObject[1],    "w_param"  );
   	
   	userData->frameBuffer[1] = InitFrameBuffer ( texture[1], width, height );

	CreateSimpleTexture2D( esContext,texture[7] );
	//printf("assigned\n");
	// Initiate framebuffer to contain the warpped image
	
	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer Object for Multiply (SHADER)                   //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
	userData->programObject[2] = InitProgram ( 2 );  
   	// Get the attribute locations
   	userData->positionLoc[2] = glGetAttribLocation ( userData->programObject[2], "a_position" );
   	userData->texCoordLoc[2] = glGetAttribLocation ( userData->programObject[2], "a_texCoord" );
   
   	// Get the sampler location
   	userData->samplerLoc[4] = glGetUniformLocation ( userData->programObject[2], "s_texture" );
   	userData->flagLoc = glGetUniformLocation ( userData->programObject[2], "f_self" );
  
   	
   	userData->frameBuffer[2] = InitFrameBuffer ( texture[2], width, height );
   	//userData->frameBuffer[3] = InitFrameBuffer ( texture[3], width, height );

	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer Object for SUM up to a row                     //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
	userData->programObject[3] = InitProgram ( 3 );  
   	// Get the attribute locations
   	userData->positionLoc[3] = glGetAttribLocation ( userData->programObject[3], "a_position" );
   	userData->texCoordLoc[3] = glGetAttribLocation ( userData->programObject[3], "a_texCoord" );
   
   	// Get the sampler location
   	userData->samplerLoc[5] = glGetUniformLocation ( userData->programObject[3], "s_texture" );
   	userData->image_sLoc[1] = glGetUniformLocation ( userData->programObject[3], "i_size" );
   	
   	userData->frameBuffer[3] = InitFrameBuffer ( texture[3], width/2, height/2 );
   	userData->frameBuffer[4] = InitFrameBuffer ( texture[4], width/4, height/4 );
   	userData->frameBuffer[5] = InitFrameBuffer ( texture[5], width/8, height/8 );
	   
	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer Object for create a triangle                   //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
	userData->programObject[4] = InitProgram ( 4 );  
   	// Get the attribute locations
   	userData->positionLoc[4] = glGetAttribLocation ( userData->programObject[4], "a_position" );

    userData->warp_pLoc[1]  = glGetUniformLocation ( userData->programObject[4],    "w_param"  );
   	// Get the sampler location
   	
   	///////////////////////////////////////////////////////////////////////////////////////
   	// Initialize Program and FrameBuffer Object for creating a square                   //  
   	//                                                                                   //
   	///////////////////////////////////////////////////////////////////////////////////////
	userData->programObject[5] = InitProgram ( 5 );  
   	// Get the attribute locations
   	userData->positionLoc[5] = glGetAttribLocation ( userData->programObject[5], "a_position" );

    userData->colorLoc  = glGetUniformLocation ( userData->programObject[5],    "sq_color"  );
   	// Get the sampler location
   
   
	userData ->shift[0] = 0.5;// = {0.0, 0.0};      
	userData ->shift[1] = 0.5;
	            
   
    userData ->color[0] = 0.0;// = {0.0, 1.0, 0.0, 1.0}; 
	userData ->color[1] = 1.0;
	userData ->color[2] = 0.0;
	userData ->color[3] = 1.0;
	
 
	for(int i = 0; i < 8; i++)
   	{
		userData->textureId[i] = texture[i];
	}
	
   	glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
   
   	printf("initialization finished\n");
   	return GL_TRUE;
}



void GradienImage ( ESContext *esContext, GLfloat *vVertices, GLushort *indices  )
{
   UserData *userData = (UserData *)esContext->userData;
   GLfloat xy1[9] = { 0.0,-1.0, 0.0,   
					  0.0, 0.0, 0.0,   
					  0.0, 1.0, 0.0};
   GLfloat xy2[9] = { 0.0, 0.0, 0.0,   
					 -1.0, 0.0, 1.0,   
					  0.0, 0.0, 0.0};
   
      
   // Bind Buffer Frame Object
   glBindFramebuffer(GL_FRAMEBUFFER, userData->frameBuffer[0]);
   glBindTexture(GL_TEXTURE_2D, userData->textureId[0]);
   
   // Bind Buffer Frame Object with texture 
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->textureId[0], 0);

   
   if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
   {
		// Set the viewport
	   glViewport ( 0, 0, esContext->width/2, esContext->height/2 );
	   
	   // Clear the color buffer
	   glClear ( GL_COLOR_BUFFER_BIT );
	
	   // Load the vertex position
	   glVertexAttribPointer ( userData->positionLoc[0], 3, GL_FLOAT, 
	                           GL_FALSE, 5 * sizeof(GLfloat), vVertices );
	   // Load the texture coordinate
	   glVertexAttribPointer ( userData->texCoordLoc[0], 2, GL_FLOAT,
	                           GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3] );
	
	   glEnableVertexAttribArray ( userData->positionLoc[0] );
	   glEnableVertexAttribArray ( userData->texCoordLoc[0] );
	
	   // Bind the texture
	   glActiveTexture ( GL_TEXTURE0 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[6] );
	   
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[0], 0 );
	   glUniformMatrix3fv ( userData->grad_fLoc1, 9, GL_FALSE, xy1 );
	   glUniformMatrix3fv ( userData->grad_fLoc2, 9, GL_FALSE, xy2 );
	   glUniform2f ( userData->image_sLoc[0], esContext->width/2, esContext->height/2 );
	
	   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
   }
   else
   {
	   printf("error in GradientImage\n");
	}
   
   //glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



void WarpImage ( ESContext *esContext, GLfloat *vVertices, GLushort *indices, float *warp_p )
{
   UserData *userData = (UserData *)esContext->userData;
      
   // Bind Buffer Frame Object
   glBindFramebuffer(GL_FRAMEBUFFER, userData->frameBuffer[1]);
   glBindTexture(GL_TEXTURE_2D, userData->textureId[1]);

   // Bind Buffer Frame Object with texture 
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->textureId[1], 0);
   
   GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   warp_p[2] = warp_p[2]*2/esContext->width;
   warp_p[5] = warp_p[5]*2/esContext->height;
   
   if(status == GL_FRAMEBUFFER_COMPLETE)
   {
		// Set the viewport
	   glViewport ( 0, 0, esContext->width/2, esContext->height/2 );
	   
	   // Clear the color buffer
	   glClear ( GL_COLOR_BUFFER_BIT );
	
	   // Use the program object 
	   //glUseProgram ( userData->programObject[0] );
	
	   // Load the vertex position
	   glVertexAttribPointer ( userData->positionLoc[1], 3, GL_FLOAT, 
	                           GL_FALSE, 5 * sizeof(GLfloat), vVertices );
	   // Load the texture coordinate
	   glVertexAttribPointer ( userData->texCoordLoc[1], 2, GL_FLOAT,
	                           GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3] );
	
	   glEnableVertexAttribArray ( userData->positionLoc[1] );
	   glEnableVertexAttribArray ( userData->texCoordLoc[1] );
	
		// Bind the texture
	   glActiveTexture ( GL_TEXTURE0 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[7] );
	
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[1], 0 );
	   
		// Bind the texture
	   glActiveTexture ( GL_TEXTURE1 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[6] );
	
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[2], 1 );
	   
	   // Bind the texture
	   glActiveTexture ( GL_TEXTURE2 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[0] );
	
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[3], 2 );

	   glUniformMatrix3fv ( userData->warp_pLoc[0], 9, GL_FALSE, warp_p );
	   
	   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
   }
   else
   {
	   printf("error in WarpImage\n");
	}
   warp_p[2] = warp_p[2]*esContext->width/2;
   warp_p[5] = warp_p[5]*esContext->height/2;
   //glBindFramebuffer(GL_FRAMEBUFFER, 0);
}




void MultiplyImage ( ESContext *esContext, GLfloat *vVertices, GLushort *indices, GLint tex1, GLint flag)
{
   UserData *userData = (UserData *)esContext->userData;
   
      
   // Bind Buffer Frame Object
   glBindFramebuffer(GL_FRAMEBUFFER, userData->frameBuffer[2]);
   glBindTexture(GL_TEXTURE_2D, userData->textureId[2]);
  
   // Bind Buffer Frame Object with texture 
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->textureId[2], 0);
	
   //status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   
   if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
   {
		// Set the viewport
	   glViewport ( 0, 0, esContext->width/2, esContext->height/2 );
	   
	   // Clear the color buffer
	   glClear ( GL_COLOR_BUFFER_BIT );
	
	   // Use the program object
	   //glUseProgram ( userData->programObject[2] );
	
	   // Load the vertex position
	   glVertexAttribPointer ( userData->positionLoc[2], 3, GL_FLOAT, 
	                           GL_FALSE, 5 * sizeof(GLfloat), vVertices );
	   // Load the texture coordinate
	   glVertexAttribPointer ( userData->texCoordLoc[2], 2, GL_FLOAT,
	                           GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3] );
	
	   glEnableVertexAttribArray ( userData->positionLoc[2] );
	   glEnableVertexAttribArray ( userData->texCoordLoc[2] );
	
	   // Bind the texture
	   glActiveTexture ( GL_TEXTURE0 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[tex1] );
	   
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[4], 0 );

	   glUniform1i ( userData->flagLoc, flag );
	   
	   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
   }
   else
   {
	   printf("error in MultiplyImage\n");
	}

}




void IntegrateImage ( ESContext *esContext, GLfloat *vVertices, GLushort *indices, GLint result_texture, GLint texture, float e  )
{
   UserData *userData = (UserData *)esContext->userData;
      
   // Bind Buffer Frame Object
   glBindFramebuffer(GL_FRAMEBUFFER, userData->frameBuffer[result_texture]);
   glBindTexture(GL_TEXTURE_2D, userData->textureId[result_texture]);
   
   // Bind Buffer Frame Object with texture 
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->textureId[result_texture], 0);
	//printf("Buffer %d\n",userData->frameBuffer[0]);
   //printf("Buffer %d\n",userData->textureId[0]);
   //GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   
   if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
   {
		// Set the viewport
	   glViewport ( 0, 0, (esContext->width)/(2*e), (esContext->height)/(2*e) );
	   
	   // Clear the color buffer
	   glClear ( GL_COLOR_BUFFER_BIT );
	
	   // Use the program object
	   //glUseProgram ( userData->programObject[1] );
	
	   // Load the vertex position
	   glVertexAttribPointer ( userData->positionLoc[3], 3, GL_FLOAT, 
	                           GL_FALSE, 5 * sizeof(GLfloat), vVertices );
	   // Load the texture coordinate
	   glVertexAttribPointer ( userData->texCoordLoc[3], 2, GL_FLOAT,
	                           GL_FALSE, 5 * sizeof(GLfloat), &vVertices[3] );
	
	   glEnableVertexAttribArray ( userData->positionLoc[3] );
	   glEnableVertexAttribArray ( userData->texCoordLoc[3] );
	
	   // Bind the texture
	   glActiveTexture ( GL_TEXTURE0 );
	   glBindTexture ( GL_TEXTURE_2D, userData->textureId[texture] );
	
	   // Set the sampler texture unit to 0
	   glUniform1i ( userData->samplerLoc[5], 0 );
	   glUniform2f ( userData->image_sLoc[1], esContext->width/e, esContext->height/e);
	
	   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
   }
   else
   {
	   printf("error in IntegrateImage\n");
	}
   
   //glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void DrawTriangle(ESContext *esContext, float *warp_p)
{
   UserData *userData = (UserData *)esContext->userData;
   GLfloat vVertices[] = {  0.0f,  0.05f, 0.0f, 
                           -0.05f, -0.05f, 0.0f,
                            0.05f, -0.05f, 0.0f };
     
   GLfloat sqVertices[] = { -0.1f,  0.1f, 0.0f,  // Position 0
							-0.1f, -0.1f, 0.0f,  // Position 1
							 0.1f, -0.1f, 0.0f,  // Position 2
                             0.1f,  0.1f, 0.0f,  // Position 3
                          };

	int width = esContext->width/2;
	int height = esContext->height/2;

   //square   
   short indices[] = {0, 1, 2, 0, 2, 3 };
   

   sqVertices[0] = sqVertices[0] + userData ->shift[0];
   sqVertices[3] = sqVertices[3] + userData ->shift[0];
   sqVertices[6] = sqVertices[6] + userData ->shift[0];
   sqVertices[9] = sqVertices[9] + userData ->shift[0];
   
   sqVertices[1] = sqVertices[1] + userData ->shift[1];
   sqVertices[4] = sqVertices[4] + userData ->shift[1];
   sqVertices[7] = sqVertices[7] + userData ->shift[1];
   sqVertices[10] = sqVertices[10] + userData ->shift[1];  
   //cout<<"real"<<shift[0]<<endl;
      // Set the viewport
   glViewport ( 0, 0, esContext->width, esContext->height );
   
   // Clear the color buffer
   glClear ( GL_COLOR_BUFFER_BIT );

   glUseProgram ( userData->programObject[5]);
   // Load the vertex data
   glVertexAttribPointer ( userData->positionLoc[5], 3, GL_FLOAT, 
	                           GL_FALSE, 3 * sizeof(GLfloat), sqVertices );
   glEnableVertexAttribArray ( userData->positionLoc[5]);
   
   glUniform4fv ( userData->colorLoc, 4, userData ->color );

   glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );

	if(	warp_p[2] != warp_p[2] )
	{
		texImg.warp_p[2] = 0.0;
		texImg.warp_p[5] = 0.0;
	}else if(abs(warp_p[2])<1.0 && abs(warp_p[5])<1.0 )
	{			     
		texImg.warp_p[2] = texImg.warp_p[2];
		texImg.warp_p[5] = texImg.warp_p[5];
    }
    else if(abs(texImg.warp_p[2])<= 0.5|| abs(texImg.warp_p[5])<= 0.5)
    {
		texImg.warp_p[2] = 5.0 * warp_p[2]/width  + texImg.warp_p[2];
		texImg.warp_p[5] = 5.0 * warp_p[5]/height + texImg.warp_p[5];
	}
	
	if(abs(texImg.warp_p[2])>0.5|| abs(texImg.warp_p[5])>0.5)
	{
		texImg.warp_p[2] = texImg.warp_p[2] - 5.0  * warp_p[2]/width;
		texImg.warp_p[5] = texImg.warp_p[5] - 5.0  * warp_p[5]/height;
	}

   if( texImg.warp_p[2] > sqVertices[0] && texImg.warp_p[2] < sqVertices[6] && texImg.warp_p[5] > sqVertices[4] && texImg.warp_p[5] < sqVertices[1])
   {
   		userData ->change_color = 1;

   }

   // Use the program object
   glUseProgram ( userData->programObject[4] );

   // Load the vertex data
   glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, vVertices );
   glEnableVertexAttribArray ( userData->positionLoc[4]);
   
   glUniformMatrix3fv ( userData->warp_pLoc[1], 9, GL_FALSE, texImg.warp_p );

   glDrawArrays ( GL_TRIANGLES, 0, 3 );
  // eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
   

}


///
// Draw a triangle using the shader pair created in Init()
//
void Draw ( ESContext *esContext )
{
    UserData *userData = (UserData *)esContext->userData;
    GLfloat vVertices[] = { -1.0f,  1.0f, 0.0f,  // Position 0
                             0.0f,  0.0f,        // TexCoord 0 
                            -1.0f, -1.0f, 0.0f,  // Position 1
                             0.0f,  1.0f,        // TexCoord 1
                             1.0f, -1.0f, 0.0f,  // Position 2
                             1.0f,  1.0f,        // TexCoord 2
                             1.0f,  1.0f, 0.0f,  // Position 3
                             1.0f,  0.0f         // TexCoord 3
                         };
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
   
	//Warp parameter
   	float warp_p[] = {   1.0, 0.0, 0.0,   //First row
					   	 0.0, 1.0, 0.0,   //Second row
					     0.0, 0.0, 1.0};  //Third row
	     
	int width = esContext->width/2;
	int height = esContext->height/2;
	int newwidth = width/8;
	int newheight = height/8;
	
	char** Hs = new char* [2];
	Hs[0] = new char [newwidth*newheight*4];
	Hs[1] = new char [newwidth*newheight*4];
	
	char* sd_delta = new char [newwidth*newheight*4];
	
	//printf("draw started\n");				     
	// Set the viewport
	//glViewport ( 0, 0, width/2, height/2 );
	   
   	glUseProgram ( userData->programObject[0] );
   	GradienImage ( esContext, vVertices, indices);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	
   	glUseProgram ( userData->programObject[2] );
   	MultiplyImage ( esContext, vVertices, indices, 0, 0 );
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram ( userData->programObject[3] );
   	IntegrateImage(esContext, vVertices, indices, 3, 2, 2);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	IntegrateImage(esContext, vVertices, indices, 4, 3, 4);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	IntegrateImage(esContext, vVertices, indices, 5, 4, 8);
   	glReadPixels(0, 0, newwidth, newheight, GL_RGBA, GL_UNSIGNED_BYTE, Hs[0]);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	
   	
   	
   	//printf("draw started2\n");	
   	glUseProgram ( userData->programObject[2] );
   	MultiplyImage ( esContext, vVertices, indices, 0, 1);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	glUseProgram ( userData->programObject[3] );
   	IntegrateImage(esContext, vVertices, indices, 3, 2, 2);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	IntegrateImage(esContext, vVertices, indices, 4, 3, 4);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	IntegrateImage(esContext, vVertices, indices, 5, 4, 8);
   	glReadPixels(0, 0, newwidth, newheight, GL_RGBA, GL_UNSIGNED_BYTE, Hs[1]);
   	glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	
   	
   	double* hinv = hessian(Hs, newwidth*newheight);


	for(int i = 0; i < 5; i++)
	{
		//printf("iterating\n");	
		// Compute Gradient for images
   		glUseProgram ( userData->programObject[1] );
   		WarpImage ( esContext, vVertices, indices, warp_p );
   		glBindFramebuffer(GL_FRAMEBUFFER, 0);
   	
   		glUseProgram ( userData->programObject[3] );
		IntegrateImage(esContext, vVertices, indices, 3, 1, 2);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		IntegrateImage(esContext, vVertices, indices, 4, 3, 4);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		IntegrateImage(esContext, vVertices, indices, 5, 4, 8);
		glReadPixels(0, 0, newwidth, newheight, GL_RGBA, GL_UNSIGNED_BYTE, sd_delta);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
   		//int max = findmax(sd_delta, 6);newheight
    	sd_update(warp_p, hinv, sd_delta, newwidth*newheight);
	}
	
	DrawTriangle(esContext, warp_p);
    
    delete [] sd_delta;
    delete [] Hs[0];
    delete [] Hs[1];
    delete [] Hs;
    //printf("%f  %f  %f\n", warp_p[8], warp_p[2], warp_p[5]);

}


///
// Cleanup
//
void ShutDown ( ESContext *esContext )
{
   UserData *userData = (UserData *)esContext->userData;

   // Delete texture object
   for(int  i = 0 ; i < 7; i++)
   {
		glDeleteTextures ( 1, &userData->textureId[i] );
	}

   // Delete program object
   for(int j = 0; j < 6; j++)
   {
		glDeleteProgram ( userData->programObject[j] );
	}
	
   free(esContext->userData);
}




// default status
static void default_status(RASPIVID_STATE *state)
{
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   // Default everything to zero
   memset(state, 0, sizeof(RASPIVID_STATE));


   // Now set anything non-zero
   state->timeout 			= 1000;    // 5s delay before take image
   state->width 			=  320;      // use a multiple of 320 (640, 1280)
   state->height 			=  240;		// use a multiple of 240 (480, 960)
   state->bitrate 			= 17000000; // This is a decent default bitrate for 1080p
   state->framerate 		= VIDEO_FRAME_RATE_NUM;
   state->immutableInput 	= 1;
   state->graymode 			= 1;		//gray by default, much faster than color (0)
   
   
   // Setup preview window defaults
   raspipreview_set_defaults(&state->preview_parameters);

   // Set up the camera_parameters to default
   raspicamcontrol_set_defaults(&state->camera_parameters);
}



/**
 *  buffer header callback function for video
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_BUFFER_HEADER_T *new_buffer;
   PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;
   RASPIVID_STATE *pstate = (RASPIVID_STATE *)pData->pstate;
   ESContext *esContext = (ESContext *)pstate->esContext;

   if (pData && esContext != NULL)
   {
     
		if (buffer->length)
      	{
      		if(nCount>0)
			{
				mmal_buffer_header_mem_lock(buffer);
				//texImg.w=pData->pstate->width;	// get image size
				//texImg.h=pData->pstate->height;
				memcpy(texImg.tmpltData,texImg.imageData,texImg.w*texImg.h);
				memcpy(texImg.imageData,buffer->data,texImg.w*texImg.h);

				nCount++;		// count frames displayed
				mmal_buffer_header_mem_unlock(buffer);
			}else if(nCount == 0)
			{
				mmal_buffer_header_mem_lock(buffer);
				memcpy(texImg.tmpltData,buffer->data,texImg.w*texImg.h);
				memcpy(texImg.imageData,buffer->data,texImg.w*texImg.h);
				mmal_buffer_header_mem_unlock(buffer);
				nCount++;
			}
      }
      else vcos_log_error("buffer null");
      
   }
   else
   {
      vcos_log_error("Received a encoder buffer callback with no state");
   }

   // release buffer back to the pool
   mmal_buffer_header_release(buffer);

   // and send one back to the port (if still open)
   if (port->is_enabled)
   {
      MMAL_STATUS_T status;

      new_buffer = mmal_queue_get(pData->pstate->video_pool->queue);

      if (new_buffer)
         status = mmal_port_send_buffer(port, new_buffer);

      if (!new_buffer || status != MMAL_SUCCESS)
         vcos_log_error("Unable to return a buffer to the encoder port");
   }
    
}


/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return 0 if failed, pointer to component if successful
 *
 */
static MMAL_COMPONENT_T *create_camera_component(RASPIVID_STATE *state)
{
	MMAL_COMPONENT_T *camera = 0;
	MMAL_ES_FORMAT_T *format;
	MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
	MMAL_STATUS_T status;
	
	/* Create the component */
	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
	
	if (status != MMAL_SUCCESS)
	{
	   vcos_log_error("Failed to create camera component");
	   goto error;
	}
	
	if (!camera->output_num)
	{
	   vcos_log_error("Camera doesn't have output ports");
	   goto error;
	}
	
	video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
	still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];
	
	//  set up the camera configuration
	{
	   MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
	   {
	      { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
	      cam_config.max_stills_w = state->width,
	      cam_config.max_stills_h = state->height,
	      cam_config.stills_yuv422 = 0,
	      cam_config.one_shot_stills = 0,
	      cam_config.max_preview_video_w = state->width,
	      cam_config.max_preview_video_h = state->height,
	      cam_config.num_preview_video_frames = 3,
	      cam_config.stills_capture_circular_buffer_height = 0,
	      cam_config.fast_preview_resume = 0,
	      cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
	   };
	   mmal_port_parameter_set(camera->control, &cam_config.hdr);
	}
	// Set the encode format on the video  port
	
	format = video_port->format;
	format->encoding_variant = MMAL_ENCODING_I420;
	format->encoding = MMAL_ENCODING_I420;
	format->es->video.width = state->width;
	format->es->video.height = state->height;
	format->es->video.crop.x = 0;
	format->es->video.crop.y = 0;
	format->es->video.crop.width = state->width;
	format->es->video.crop.height = state->height;
	format->es->video.frame_rate.num = state->framerate;
	format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;
	
	status = mmal_port_format_commit(video_port);
	if (status)
	{
	   vcos_log_error("camera video format couldn't be set");
	   goto error;
	}
	
	// PR : plug the callback to the video port 
	status = mmal_port_enable(video_port, video_buffer_callback);
	if (status)
	{
	   vcos_log_error("camera video callback2 error");
	   goto error;
	}

   // Ensure there are enough buffers to avoid dropping frames
   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


   // Set the encode format on the still  port
   format = still_port->format;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;
   format->es->video.width = state->width;
   format->es->video.height = state->height;
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->width;
   format->es->video.crop.height = state->height;
   format->es->video.frame_rate.num = 1;
   format->es->video.frame_rate.den = 1;

   status = mmal_port_format_commit(still_port);
   if (status)
   {
      vcos_log_error("camera still format couldn't be set");
      goto error;
   }

	
	//PR : create pool of message on video port
	MMAL_POOL_T *pool;
	video_port->buffer_size = video_port->buffer_size_recommended;
	video_port->buffer_num = video_port->buffer_num_recommended;
	pool = mmal_port_pool_create(video_port, video_port->buffer_num, video_port->buffer_size);
	if (!pool)
	{
	   vcos_log_error("Failed to create buffer header pool for video output port");
	}
	state->video_pool = pool;

	/* Ensure there are enough buffers to avoid dropping frames */
	if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
	   still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
	
	/* Enable component */
	status = mmal_component_enable(camera);
	
	if (status)
	{
	   vcos_log_error("camera component couldn't be enabled");
	   goto error;
	}
	
	raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);
	
	state->camera_component = camera;
	
	return camera;

error:

   if (camera)
      mmal_component_destroy(camera);

   return 0;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPIVID_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_encoder_component(RASPIVID_STATE *state)
{
   // Get rid of any port buffers first
   if (state->video_pool)
   {
      mmal_port_pool_destroy(state->encoder_component->output[0], state->video_pool);
   }

   if (state->encoder_component)
   {
      mmal_component_destroy(state->encoder_component);
      state->encoder_component = NULL;
   }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
   MMAL_STATUS_T status;

   status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

   if (status == MMAL_SUCCESS)
   {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS)
         mmal_connection_destroy(*connection);
   }

   return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port)
{
   if (port && port->is_enabled)
      mmal_port_disable(port);
}

/**
 * Handler for sigint signals
 *
 * @param signal_number ID of incoming signal.
 *
 */
static void signal_handler(int signal_number)
{
   // Going to abort on all signals
   vcos_log_error("Aborting program\n");

   // TODO : Need to close any open stuff...how?

   exit(255);
}

/**
 * main
 */
int main(int argc, const char **argv)
{
	// Our main data storage vessel..
	RASPIVID_STATE state;
	
	MMAL_STATUS_T status; //= -1;
	MMAL_PORT_T *camera_video_port = NULL;
	MMAL_PORT_T *camera_still_port = NULL;
	MMAL_PORT_T *preview_input_port = NULL;
	MMAL_PORT_T *encoder_input_port = NULL;
	MMAL_PORT_T *encoder_output_port = NULL;
	
	time_t timer_begin,timer_end;
	double secondsElapsed;
	
	// main for OpenGL ES
	ESContext esContext;
    UserData  userData;

	bcm_host_init();
	signal(SIGINT, signal_handler);
	default_status(&state);
	
	int height = state.height;
	int width = state.width;
	//graphics_get_display_size(0 /* LCD */, &width, &height);
	//state.height = height;
	//state.width  = width;
    esInitContext ( &esContext );
    esContext.userData = &userData;
    
	esCreateWindow ( &esContext, "Simple Texture 2D", width*2, height*2, ES_WINDOW_RGB);
	
	//Draw( &esContext);
	printf("error here\n");
    if ( !Init ( &esContext ) )
       return 0;
    
    esRegisterDrawFunc ( &esContext, Draw );
    esRegisterUpdateFunc( &esContext, textureUpdate );
	state.esContext = &esContext;
	texImg.h = height;
	texImg.w = width;
	texImg.imageData = new char [height*width];
	texImg.tmpltData = new char [height*width];
	float warp_p[] = {   1.0, 0.0, 0.0,   //First row
					   	 0.0, 1.0, 0.0,   //Second row
					     0.0, 0.0, 1.0};  //Third row
	texImg.warp_p = warp_p;
	// create camera
	if (!create_camera_component(&state))
	{
	   vcos_log_error("%s: Failed to create camera component", __func__);
	}
	else if (!raspipreview_create(&state.preview_parameters))
	{
	   vcos_log_error("%s: Failed to create preview component", __func__);
	   destroy_camera_component(&state);
	}
	else
	{
		PORT_USERDATA callback_data;
		
		camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
		camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
	   
		VCOS_STATUS_T vcos_status;
		
		callback_data.pstate = &state;
		
		vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);
		vcos_assert(vcos_status == VCOS_SUCCESS);
		
		// assign data to use for callback
		camera_video_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;
        
        // init timer
  		time(&timer_begin); 

        
       // start capture
		if (mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
		{
		   return 0;
		}
		
		// Send all the buffers to the video port
		int num = mmal_queue_length(state.video_pool->queue);
		printf("num:%d",num);
		int q;
		for (q=0;q<num;q++)
		{
		   MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.video_pool->queue);
		
		   if (!buffer)
		   		vcos_log_error("Unable to get a required buffer %d from pool queue", q);
		
			if (mmal_port_send_buffer(camera_video_port, buffer)!= MMAL_SUCCESS)
		    	vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
		}
		// Now wait until we need to stop
		
		vcos_sleep(state.timeout);
		esMainLoop ( &esContext );
		ShutDown ( &esContext );
error:

		//mmal_status_to_int(status);
		
		
		// Disable all our ports that are not handled by connections
		//check_disable_port(camera_still_port);
		
		if (state.camera_component)
		   mmal_component_disable(state.camera_component);
		
		//destroy_encoder_component(&state);
		raspipreview_destroy(&state.preview_parameters);
		destroy_camera_component(&state);
		
		}
		if (status != 0)
		raspicamcontrol_check_configuration(128);
		
		time(&timer_end);  /* get current time; same as: timer = time(NULL)  */
		
		secondsElapsed = difftime(timer_end,timer_begin);
		
		printf ("%.f seconds for %d frames : FPS = %f\n", secondsElapsed,nCount,(float)((float)(nCount)/secondsElapsed));
		
   return 0;
}
