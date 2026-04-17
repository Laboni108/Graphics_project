#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform float u_starBright;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    FragPos       = worldPos.xyz;
    Normal        = normalize(transpose(inverse(mat3(model))) * normal);
    TexCoord      = texCoord;
    gl_Position   = projection * view * worldPos;
    gl_PointSize  = 1.8 + u_starBright * 3.2;
}