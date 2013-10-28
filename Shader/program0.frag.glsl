precision highp float; 
varying vec2 v_texCoord;
uniform mat3 g_filter1;
uniform mat3 g_filter2;
uniform vec2 i_size;
uniform sampler2D s_texture;
void main()       
{
    vec4 localSum1 = vec4(0.0);
    vec4 localSum2 = vec4(0.0);

    for(int c = 0; c < 3; c++)
    {
		for(int r = 0; r < 3; r++)
		{
			vec2 offs = vec2(float(c-1), float(r-1))/i_size;//vec2(640,480)
			vec4 img = texture2D(s_texture, v_texCoord + offs);
			localSum1 += g_filter1[c][r] * img;
			localSum2 += g_filter2[c][r] * img;
		}
    }
    vec4 color = vec4(localSum1.x/2.0, -(localSum1.x)/2.0, localSum2.x/2.0, -(localSum2.x)/2.0);//-localSum2.x/2.0

    gl_FragColor = color;//color;
}          
