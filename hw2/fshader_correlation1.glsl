#version 330
//fragment shader to compute correlation values between template image and
//successive neighborhoods of a base image.
//precondition: both base and template images must be grayscale
//postcondition: fragment value is the normalized correlation between the template
//and the u_Wsize x u_Hsize neighborhood top right of v_TexCoord in the base image

in	vec2	  v_TexCoord;	  //input variable to get interpolted
                                  //texture coordinate from rasterizer output

uniform int       u_Wsize;	  //template width value
uniform int       u_Hsize;        //template height value
uniform float     u_StepX;        //horizontal step size
uniform float     u_StepY;        //vertical step size
uniform float     u_KStepX;       //horizontal kernel step
uniform float     u_KStepY;       //vertical kernel step
uniform float     u_magnitudeT;   //magnitude of the template
uniform	sampler2D u_Sampler;	  //sampler for the base image
uniform sampler2D u_templateSampler;//sampler for the kernel texture
out     float     correlation;    //contains the correlation value for the current pixel


void main() {
	
        float dot_ik = 0.0;
        float dot_ii = 0.0;
        float dot_kk = 0.0;
        vec2  tc     = v_TexCoord;
        float i      = 0.0;         //image value
        float k      = 0.0;         //kernel value

        //we would like to sart from the lower left corner
        //where we have the origin of texture coordinates
        for(int y = 0; y<= u_Hsize;++y){
            for(int x= 0; x<= u_Wsize; ++x){

                i = texture2D(u_Sampler,vec2(tc.x + x*u_StepX, tc.y + y*u_StepY)).r;
                k = texture2D(u_templateSampler,vec2(x*u_KStepX,y*u_KStepY)).r;

                dot_ik += k*i;
                dot_ii += i*i;

            }
        }

        correlation = dot_ik/ (sqrt(dot_ii)*u_magnitudeT);

}
