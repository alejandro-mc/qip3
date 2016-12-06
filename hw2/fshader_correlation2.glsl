#version 330
//fragment shader to display a template image over a base image
//precondition: template texture has GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T
//set to GL_CLAMP_TO_BORDER. And GL_TEXTURE_BORDER_COLOR is set to black.
//postcondition: the pixel is the same color as the base image if it lies
//within the template's boundaries otherwise it is half the value of the
//corresponding pixel in the base image

in	vec2	  v_TexCoord;       //input variable to get interpolted
                                    //texture coordinate from rasterizer output

uniform float     u_tmpltToImgRatioWidth;
uniform float     u_tmpltToImgRatioHeight;
uniform float     u_tmpltX;         //template X coordinate in relation to the base image
uniform float     u_tmpltY;         //template Y coordinate in relation to the base image
uniform	sampler2D u_Sampler;	    //sampler for the base image's texture

uniform sampler2D u_templateSampler;//sampler for the template image's texture
                                    //this texture is assumed to be grayscale
                                    //and can be GL_RED

void main() {

    //specify the origin (lower left corner) of the template texture
    //with respect to the input image texture
    vec2 tOrig = v_TexCoord - vec2(u_tmpltX,u_tmpltY);

    //coordinate in the large image is mapped to coordinate in the template
    //so that s of template is tOrig.s / u_tmpltToImgRatioWidth
    //and t of template is tOrig.t / u_tmpltToImgRatioHeight
    //finally mix the sampled pixels
    float tirw = u_tmpltToImgRatioWidth;
    float tirh = u_tmpltToImgRatioHeight;
    gl_FragColor = 0.5*texture2D(u_Sampler,v_TexCoord)
                  +0.5*texture2D(u_templateSampler,vec2(tOrig.s / tirw,tOrig.t / tirh)).rrr;
}
