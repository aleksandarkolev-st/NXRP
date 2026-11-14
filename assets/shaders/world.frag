#version 120
uniform sampler2D surface;
uniform bool textured;
uniform float darkness;
uniform float flicker;
uniform float flash;
uniform float wetness;
uniform bool materialMaps;
uniform sampler2D normalMap;
uniform sampler2D materialMap;
uniform float normalStrength;
uniform float refugeLighting;
uniform vec3 refugeTint;
uniform bool footprintsEnabled;
uniform sampler2D footprintLeft;
uniform sampler2D footprintRight;
uniform vec4 footsteps[24];
varying vec3 eyePosition;
varying vec3 eyeNormal;
varying vec4 tint;
varying vec3 worldPosition;
void main()
{
    vec3 normal = normalize(eyeNormal);
    vec3 material=vec3(1.0,.8,0.0);
    float water=wetness;
    if(materialMaps)
    {
        vec2 uv=gl_TexCoord[0].xy;
        material=texture2D(materialMap,uv).rgb;
        vec3 qx=dFdx(eyePosition),qy=dFdy(eyePosition);
        vec2 sx=dFdx(uv),sy=dFdy(uv);
        float determinant=sx.x*sy.y-sx.y*sy.x;
        if(abs(determinant)>.0000001)
        {
            vec3 tangent=normalize((qx*sy.y-qy*sx.y)/determinant);
            vec3 bitangent=normalize((-qx*sy.x+qy*sx.x)/determinant);
            vec3 detail=texture2D(normalMap,uv).xyz*2.0-1.0;
            detail.xy*=normalStrength;
            normal=normalize(tangent*detail.x+bitangent*detail.y+normal*detail.z);
        }
        // A water film collects in occluded mud rather than varnishing leaves.
        water*=smoothstep(.12,.42,1.0-material.r)*smoothstep(.60,.95,material.g);
    }
    float roughness=clamp(mix(material.g,.14,water),.12,.98);
    float distanceToEye = max(length(eyePosition), 0.001);
    vec3 towardLight = -eyePosition / distanceToEye;
    float diffuse = max(dot(normal, towardLight), 0.0);
    float cone = smoothstep(0.60 + darkness * 0.07, 0.90, dot(-towardLight, vec3(0.0, 0.0, -1.0)));
    float attenuation = 1.0 / (1.0 + distanceToEye * 0.10 + distanceToEye * distanceToEye * 0.035);
    float moon = max(dot(normal, normalize(gl_LightSource[1].position.xyz)), 0.0);
    vec3 ambient = mix(vec3(0.12,0.15,0.14)*(1.0-darkness*.45),vec3(.43,.34,.25)*refugeTint,refugeLighting);
    vec3 moonlight = vec3(0.17, 0.24, 0.28) * moon * 0.65;
    vec3 stormlight = vec3(0.34, 0.40, 0.52) * flash * (0.45 + moon * 0.55);
    vec3 flashlight = vec3(1.15, 0.92, 0.62) * diffuse * cone * attenuation * flicker;
    vec3 shelterOffset = gl_LightSource[2].position.xyz - eyePosition;
    float shelterDistance = max(length(shelterOffset), 0.001);
    float shelterDiffuse = max(dot(normal, shelterOffset / shelterDistance), 0.0);
    vec3 shelter = gl_LightSource[2].diffuse.rgb * (0.20 + shelterDiffuse * 0.80)
        / (1.0 + shelterDistance * 0.28 + shelterDistance * shelterDistance * 0.13);
    vec4 texel = textured ? texture2D(surface, gl_TexCoord[0].xy) : vec4(1.0);
    vec3 albedo=texel.rgb*tint.rgb*mix(1.0,.68,water);
    if(footprintsEnabled)
    {
        float footprint=0.0;
        for(int i=0;i<24;++i)
        {
            vec2 delta=worldPosition.xz-footsteps[i].xy;
            if(abs(delta.x)>.22 || abs(delta.y)>.22)continue;
            vec2 local=vec2(delta.x*footsteps[i].z-delta.y*footsteps[i].w,
                delta.x*footsteps[i].w+delta.y*footsteps[i].z);
            vec2 mark=local/vec2(.13,.28)+.5;
            if(mark.x<0.0 || mark.x>1.0 || mark.y<0.0 || mark.y>1.0)continue;
            float alpha=mod(float(i),2.0)<.5?texture2D(footprintLeft,mark).a:texture2D(footprintRight,mark).a;
            footprint=max(footprint,alpha);
        }
        albedo=mix(albedo,vec3(.028,.021,.017),footprint*.78);
    }
    vec3 color=albedo*((ambient+moonlight)*mix(.45,1.0,material.r)+stormlight+flashlight+shelter);
    float exponent=clamp(2.0/pow(roughness,4.0)-2.0,2.0,256.0);
    float sheen=pow(max(dot(normal,towardLight),0.0),exponent)*cone*attenuation*flicker;
    color+=vec3(.70,.61,.47)*sheen*(.025+water*.65)*(1.0-roughness*.6);
    float fogAmount = 1.0 - exp(-pow(gl_Fog.density * distanceToEye, 2.0));
    gl_FragColor = vec4(mix(color, gl_Fog.color.rgb, clamp(fogAmount, 0.0, 1.0)), texel.a * tint.a);
}
