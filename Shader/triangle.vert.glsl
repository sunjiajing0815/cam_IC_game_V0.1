attribute vec4  a_position; 
uniform mat3 w_param;
void main()                  
{                           
      //vec3 pos1 = vec3(a_position.x, a_position.y, 1.0);
      //vec3 position = pos1 *  w_param;
      vec4 vPosition = vec4(a_position.x+ w_param[0][2], a_position.y + w_param[1][2], 0.0, 1.0);;
      //vPosition.xy = position.xy;
     // vPosition.z = 0.0;
      //vPosition.w = 1.0;
      gl_Position = vPosition;
}
                           
