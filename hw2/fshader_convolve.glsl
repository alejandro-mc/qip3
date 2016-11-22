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


void main() {
	
	vec3 avg = vec3(0.0);
	vec2 tc  = v_TexCoord;
	int  w2  = u_Wsize / 2;
        int  h2  = u_Hsize / 2;
        int  k   = 0;

        for(int j = -h2; j<=h2;++j){
            for(int i=-w2; i<=w2; ++i){
                avg += texture2D(u_kernelSampler,vec2(0.5 + i*u_KStepX,0.5 + j*u_KStepY)).r *
                        texture2D(u_Sampler,vec2(tc.x + i*u_StepX, tc.y + j*u_StepY)).rgb;
            }
        }

        //avg = avg / (u_Wsize * u_Hsize);
        gl_FragColor = vec4(avg, 1.0);
}
