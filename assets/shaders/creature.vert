#version 120
uniform float gait;
uniform float stride;
uniform float time;
uniform float chasing;
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec4 tint;
varying vec3 worldPosition;
void main()
{
    vec4 vertex = gl_Vertex;
    float breathing = sin(time * 1.3) * 0.012 * smoothstep(0.65, 1.2, vertex.y);
    vertex.x *= 1.0 + breathing;
    vertex.z *= 1.0 + breathing;
    float side = smoothstep(-0.12, 0.12, vertex.x) * 2.0 - 1.0;
    float leg = (1.0 - smoothstep(0.55, 0.95, vertex.y))
        * (1.0 - smoothstep(0.15, 0.25, abs(vertex.x)));
    float arm = smoothstep(0.15, 0.30, abs(vertex.x)) * smoothstep(0.30, 0.70, vertex.y);
    float swing = sin(gait) * stride;
    float angle = swing * side * (leg * 0.34 - arm * 0.25);
    float pivot = mix(0.82, 1.20, clamp(arm, 0.0, 1.0));
    float c = cos(angle), s = sin(angle);
    float y = vertex.y - pivot;
    float z = vertex.z;
    vertex.y = pivot + y * c - z * s;
    vertex.z = y * s + z * c;
    vertex.y += abs(sin(gait)) * stride * 0.015;
    vertex.x += sin(gait * 0.5) * 0.012 * vertex.y;
    float head = smoothstep(1.15, 1.48, vertex.y);
    float shoulders = smoothstep(.75, 1.3, vertex.y);
    vertex.x += head * (sin(time * 1.7) * .018 + sin(time * 4.9) * .005);
    vertex.z += shoulders * chasing * .07 + head * sin(gait * 0.5) * stride * .025;
    vertex.y -= head * chasing * .035;
    vec3 animatedNormal = vec3(gl_Normal.x, gl_Normal.y * c - gl_Normal.z * s,
                               gl_Normal.y * s + gl_Normal.z * c);
    vec4 position = gl_ModelViewMatrix * vertex;
    eyePosition = position.xyz;
    eyeNormal = normalize(gl_NormalMatrix * animatedNormal);
    tint = gl_Color;
    worldPosition=gl_Vertex.xyz;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = gl_ProjectionMatrix * position;
}
