#version 330

in	vec2	  v_TexCoord;	// varying variable for passing texture coordinate from vertex shader

uniform int       u_Wsize;	  // filter width value
uniform int       u_Hsize;        // filter height value
uniform float	  u_StepX;        //horizontal step size
uniform float     u_StepY;        //vertical step size
uniform float     u_KStepX;       //horizontal kernel step
uniform float     u_KStepY;       //vertical kernel step
uniform	sampler2D u_Sampler;	  // uniform variable for the texture image
uniform sampler2D u_kernelSampler;// sampler for the kernel texture
//out     float     correlation;


void main() {
	
        float dot_ik = 0.0;
        float dot_ii = 0.0;
        float dot_kk = 0.0;
	vec2 tc  = v_TexCoord;
        //int  w2  = u_Wsize / 2;
        //int  h2  = u_Hsize / 2;
        //int  k   = 0;
        float   i   = 0.0;//image value
        float   k   = 0.0;        //kernel value

        //we would like to sart from the lower left corner
        //where we have the origin of texture coordinates
        for(int y = 0; y<= u_Hsize;++y){
            for(int x= 0; x<= u_Wsize; ++x){

                i = texture2D(u_Sampler,vec2(tc.x + x*u_StepX, tc.y + y*u_StepY)).r;
                k = texture2D(u_kernelSampler,vec2(x*u_KStepX,y*u_KStepY)).r;

                dot_ik += k*i;
                dot_ii += i*i;
                dot_kk += k*k;
            }
        }

        gl_FragColor = vec4(dot_ik/ (sqrt(dot_ii)*sqrt(dot_kk)));
        //gl_FragColor = vec4(vec3(((dot_ik*dot_ik) / dot_ii) / dot_kk), 1.0);
        //gl_FragColor = vec4(0.7654);

}
