#ifndef GLSL_BXDFS_MIX
#define GLSL_BXDFS_MIX

vec3 mix_eval(const vec3 brdf0, const vec3 brdf1, const float ratio){
	return mix(brdf0, brdf1, ratio);
}
float mix_pdf(const float pdf0, const float pdf1, const float ratio){
	return mix(pdf0, pdf1, ratio);
}
uint mix_sample(const float ratio, const float rand){
	/* select brdf 1 */ if(rand < ratio) return 1;
	/* select brdf 0 */ else return 0;
}
vec4 mix_sample(const vec4 dir_pdf0, const vec4 dir_pdf1, const float ratio, const float rand){
	/* select brdf 1 */ if(rand < ratio) return dir_pdf1;
	/* select brdf 0 */ else return dir_pdf0;
}

#endif /* GLSL_BXDFS_MIX */