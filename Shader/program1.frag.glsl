precision highp float; 
varying vec2 v_texCoord;
uniform mat3 w_param;
uniform sampler2D img_texture; 
uniform sampler2D tmplt_texture;
uniform sampler2D g_texture;
void main()       
{
	//float xf, yf, zf, wf;
	vec3 t1 = vec3(v_texCoord, 1.0);
	vec3 t2 = t1 * w_param  ;
	vec2 g_texCoord = vec2(v_texCoord.x,1.0-v_texCoord.y);
	vec4 Wimg = texture2D( img_texture, t2.xy);//
	vec4 tmplt = texture2D( tmplt_texture, v_texCoord );
	vec4 g = texture2D( g_texture, g_texCoord );
	
	float error = (Wimg.x - tmplt.x) * 255.0;//(Wimg.x - tmplt.x)
	float errorx = (g.x - g.y) * 255.0 * error + 32896.0;//g.x * 255.0 * error(g.x - g.y)
	float errorz = (g.z - g.w) * 255.0 * error + 32896.0;//g.z * 255.0 * error
	float x = floor(errorx/256.0);// (g.x * error +32768.0)/256.0
	float yf = errorx - x * 256.0;
	float z = floor(errorz/256.0);
	float wf = errorz - z * 256.0;

	gl_FragColor = vec4(x/255.0, yf/255.0, z/255.0, wf/255.0);//float(x)/255.0yf  
}                                      
