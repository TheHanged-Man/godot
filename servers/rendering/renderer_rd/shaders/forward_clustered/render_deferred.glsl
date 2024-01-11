#[vertex]

#version 450

vec2 positions[6] = vec2[](                          
					vec2(-1.0, -1.0),                      
					vec2(1.0, -1.0),                              
					vec2(-1.0, 1.0),                      
					vec2(-1.0, 1.0),                      
					vec2(1.0, -1.0),                         
					vec2(1.0, 1.0));   
					
vec2 uvs[6] = vec2[](                 
					vec2(0.0,0.0),                     
					vec2(1.0, 0.0),                      
					vec2(0.0, 1.0),                               
					vec2(0.0, 1.0),                               
					vec2(1.0, 0.0),                               
					vec2(1.0, 1.0));    
					
layout(location = 0) out vec2 uv;

layout(set = 0, binding = 0) uniform sampler _sampler;
layout(set = 0, binding = 1) uniform texture2D custom_color;  
layout(set = 0, binding = 2) uniform texture2D custom_tex0;  


void main() {  
	 gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	uv=uvs[gl_VertexIndex];
}

#[fragment]

#version 450	

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler _sampler;
layout(set = 0, binding = 1) uniform texture2D custom_color;   
layout(set = 0, binding = 2) uniform texture2D custom_tex0;   
		
void main() {
	vec3 color = texture(sampler2D(custom_color,_sampler),uv,0).rgb;
	vec3 normal = texture(sampler2D(custom_tex0,_sampler),uv,0).rgb;

	float fix0=0.9;
	float fix1=0.1;

	
	{
#CODE : PROCESS
	}

	outColor=vec4(fix0*color+fix1*normal,1.0);
}
