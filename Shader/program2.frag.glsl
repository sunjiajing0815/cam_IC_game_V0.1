precision highp float; 
varying vec2 v_texCoord;
uniform sampler2D s_texture;
uniform int f_self;
void main()       
{
    vec4 color = texture2D(s_texture, v_texCoord);
    float product2 = 0.0;
    //vec4 color2 = texture2D(s_texture2, v_texCoord);
    
    float product1 =  (color.x - color.y) * (color.z - color.w) * 255.0 * 255.0 + 32896.0;//+ 32768.0;

    float high1 = floor(product1/256.0);
    float low1  = product1 - high1 * 256.0;
    
    if(f_self == 0)
    {
		 product2 = (color.x - color.y) * (color.x - color.y) * 255.0 * 255.0 + 32896.0;// + 32768.0; 
	}
    else  
    {
		product2 = (color.z - color.w) * (color.z - color.w) * 255.0 * 255.0 + 32896.0;//+ 32768.0;float(high2)/255.0low2/255.0
    }
    float high2 =floor(product2/256.0);
    float low2  = product2 - floor(high2) * 256.0;
    
    vec4 result = vec4(high2/255.0, low2/255.0, high1/255.0, low1/255.0);
    
    gl_FragColor = result;//color;
}          
