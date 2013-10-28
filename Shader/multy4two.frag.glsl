precision highp float; 
varying vec2 v_texCoord;
uniform sampler2D s_texture1;
uniform sampler2D s_texture2;
void main()       
{
    vec4 color1 = texture2D(s_texture1, v_texCoord);
    vec4 color2 = texture2D(s_texture2, v_texCoord);
    
    vec4 product1 = color1 * color2 * 255.0 * 255.0 + 32768.0;
    
    
    int high1 = int(product1.x)/256;
    float low1  = product1.x - float(high1) * 256.0;
    
    vec4 product2 = color1 * color1 * 255.0 * 255.0 + 32768.0;
    
    int high2 = int(product2.x)/256;
    float low2  = product2.x - float(high2) * 256.0;
    
    vec4 result = vec4(float(high2)/255.0, low2/255.0, float(high1)/255.0, low1/255.0);
    
    gl_FragColor = result;//color;
}          
