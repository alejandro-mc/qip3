#version 330

in	vec2	  v_TexCoord;	// varying variable for passing texture coordinate from vertex shader

uniform float     u_tmpltToImgRatioWidth;
uniform float     u_tmpltToImgRatioHeight;
uniform float     u_tmpltX;
uniform float     u_tmpltY;
uniform	sampler2D u_Sampler;	  // uniform variable for the texture image
uniform sampler2D u_templateSampler;


void main() {

    //specify the origin (lower left corner) of the template texture
    //with respect to the input image texture
    vec2 tOrig = v_TexCoord - vec2(u_tmpltX,u_tmpltY);

    //map coordinate in the large image to coordinate in the template
    float tirw = u_tmpltToImgRatioWidth;
    float tirh = u_tmpltToImgRatioHeight;
    gl_FragColor = 0.5*texture2D(u_Sampler,v_TexCoord)
                  +0.5*texture2D(u_templateSampler,vec2(tOrig.s / tirw,tOrig.t / tirh));
}
