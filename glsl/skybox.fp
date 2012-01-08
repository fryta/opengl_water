#version 330

uniform samplerCube cubeMap;

in vec3 texcoord;

out vec4 rslt;

void main(void)
{ 
    rslt = texture(cubeMap, texcoord);
}
    