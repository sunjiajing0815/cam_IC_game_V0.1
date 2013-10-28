precision mediump float; 
varying vec2 v_texCoord;
uniform mat3 w_param;
uniform sampler2D img_texture; 
uniform sampler2D tmplt_texture;
uniform sampler2D g_texture;
void main()       
{
	vec3 t1 = vec3(v_texCoord, 1.0);
	vec3 t2 = t1 * w_param;
	
	vec4 Wimg = texture2D( img_texture, t2.xy );
	vec4 tmplt = texture2D( tmplt_texture, v_texCoord );
	vec4 g = texture2D( g_texture, v_texCoord );
	
	float error = img.x - tmplt.x;
	int x = (g.x * error + 32768)/256.0;
	int y = (g.x * error + 32768) - x * 256.0;
	int z = (g.z * error + 32768)/256.0;
	int w = (g.z * error + 32768) - z * 256.0;
	gl_FragColor = vec4(x, y, z, w);
}                                      
