#version 120
uniform float time;
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec4 tint;
varying vec3 worldPosition;
void main()
{
    vec4 vertex = gl_Vertex;
    float motion = sin(time * 1.15 + vertex.x * 1.7 + vertex.z * 1.3);
    float tip = gl_MultiTexCoord0.y;
    vertex.x += motion * 0.018 * tip;
    vertex.z += sin(time * 0.83 + vertex.z * 1.6) * 0.012 * tip;
    vec4 position = gl_ModelViewMatrix * vertex;
    eyePosition = position.xyz;
    eyeNormal = normalize(gl_NormalMatrix * gl_Normal);
    tint = gl_Color;
    worldPosition=vertex.xyz;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = gl_ProjectionMatrix * position;
}
