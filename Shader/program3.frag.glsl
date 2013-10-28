precision highp float; 
varying vec2 v_texCoord;
uniform vec2 i_size;
uniform sampler2D s_texture;
void main()       
{
    vec4 localSum = vec4(0.0);

    for(int c = 0; c < 2; c++)
    {
		for(int r = 0; r < 2; r++)
		{
			vec2 offs = vec2(float(c), float(r))/i_size;//vec2(640,480)

			localSum +=texture2D(s_texture, v_texCoord + offs);// vec4(125.0/255.0, 125.0/255.0, 125.0/255.0,125.0/255.0)
		}
    }
    //vec4 color = localSum/25.0;
    float a = localSum.x * 16320.0  + localSum.y* 63.75;
    
    int high1 = int(a/256.0);
    float low1  = a - float(high1) * 256.0;
    
    float b = localSum.z* 16320.0   + localSum.w* 63.75;
    
    int high2 = int(b/256.0);
    float low2  = b - float(high2) * 256.0;
    

    gl_FragColor = vec4(float(high1)/255.0, low1/255.0, float(high2)/255.0, low2/255.0);//color;125.0/255.0, 10.0/255.0, 125.0/255.0, 20.0/255.0
}          
