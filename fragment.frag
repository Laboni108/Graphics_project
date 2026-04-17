#version 330 core
in  vec3 FragPos;
in  vec3 Normal;
in  vec2 TexCoord;

uniform vec3      objectColor;
uniform vec3      viewPos;
uniform int       isEmissive;
uniform int       isBackground;
uniform int       matType;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform float     u_moonBlend;
uniform float     u_starBright;

uniform float dayFactor;
uniform int   isRaining;
uniform vec3  skyColor;

uniform vec3  sunDir;
uniform vec3  sunColor;
uniform float sunIntensity;

uniform vec3  moonDir;
uniform vec3  moonColor;
uniform float fogDensity;

uniform vec3  firePos;
uniform vec3  fireColor;
uniform float fireIntensity;

uniform vec3  lampPos[12];
uniform float lampIntensity;
uniform int   numLamps;
uniform float time;
uniform float thunderEffect;

uniform vec3  carHeadlightPos;
uniform vec3  carHeadlightDir;
uniform int   carHeadlightOn;
uniform vec3  carHeadlightColor;
uniform float carHeadlightInner;
uniform float carHeadlightOuter;

uniform float lightDirOn;
uniform float lightPointOn;
uniform float lightSpotOn;
uniform float lightAmbOn;
uniform float lightDiffOn;
uniform float lightSpecOn;

out vec4 FragColor;

// ── Hash / Noise ──────────────────────────────────────────────────────────
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
vec2 hash2(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1,311.7)),
                          dot(p, vec2(269.5,183.3)))) * 43758.5453);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(i),         hash(i+vec2(1,0)),f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
}
float fBm(vec2 p) {
    float v=0.0,amp=0.5,freq=1.0;
    for(int i=0;i<4;i++){v+=amp*vnoise(p*freq);freq*=2.13;amp*=0.48;}
    return v;
}
vec2 voronoi(vec2 uv) {
    vec2 i=floor(uv),f=fract(uv);
    float minDist=8.0;
    vec2 minCell=vec2(0.0),minPoint=vec2(0.0);
    for(int y=-1;y<=1;y++){
        for(int x=-1;x<=1;x++){
            vec2 cell=vec2(float(x),float(y));
            vec2 rnd=hash2(i+cell);
            vec2 point=cell+0.5+0.42*sin(6.2831*rnd);
            float d=length(point-f);
            if(d<minDist){
                minDist=d;
                minCell=i+cell;
                minPoint=point;
            }
        }
    }
    float edgeDist=8.0;
    for(int y=-1;y<=1;y++){
        for(int x=-1;x<=1;x++){
            vec2 cell=vec2(float(x),float(y));
            vec2 rnd=hash2(i+cell);
            vec2 point=cell+0.5+0.42*sin(6.2831*rnd);
            vec2 diff=point-minPoint;
            if(dot(diff,diff)>0.001){
                vec2 mid=(minPoint+point)*0.5;
                vec2 edgeDir=normalize(point-minPoint);
                float d=dot(mid-f,edgeDir);
                edgeDist=min(edgeDist,d);
            }
        }
    }
    return vec2(edgeDist,hash(minCell));
}

// ── Base Colour ───────────────────────────────────────────────────────────
vec3 getBaseColor()
{
    // Grass
    if(matType==1){
        vec2 uv=FragPos.xz;
        float large=fBm(uv*0.07),medium=fBm(uv*0.30),fine=fBm(uv*1.10),micro=vnoise(uv*4.5);
        float combined=large*0.42+medium*0.30+fine*0.20+micro*0.08;
        vec3 blend=abs(Normal);
        blend=max(blend,0.00001);
        blend/=(blend.x+blend.y+blend.z);
        float texScale=0.15;
        vec3 tx=texture(tex0,FragPos.yz*texScale).rgb;
        vec3 ty=texture(tex0,FragPos.xz*texScale).rgb;
        vec3 tz=texture(tex0,FragPos.xy*texScale).rgb;
        vec3 texColor=tx*blend.x+ty*blend.y+tz*blend.z;
        float darken=0.72+combined*0.55;
        vec3 g=texColor*darken;
        g=mix(g,g*vec3(0.85,1.0,0.80),smoothstep(0.55,0.75,combined)*0.35);
        if(isRaining==1)g*=0.72;
        return g;
    }
    // Cobblestone
    if(matType==2){
        vec2 uv=FragPos.xz*3.5;
        vec2 voro=voronoi(uv);
        float edgeDist=voro.x,cellHash=voro.y;
        float gapW=0.10,inMortar=1.0-smoothstep(gapW*0.3,gapW,edgeDist);
        float bri=0.28+cellHash*0.24,warm=fract(cellHash*17.3),aged=fract(cellHash*5.7);
        vec3 stoneBase=vec3(bri+warm*0.06-0.02,bri+0.01,bri-warm*0.05+0.02);
        float grain=vnoise(FragPos.xz*14.0)*0.06-0.03;
        stoneBase+=grain;
        float mossNoise=fBm(FragPos.xz*0.9+vec2(3.3,7.1));
        float mossAmount=smoothstep(0.55,0.82,mossNoise)*smoothstep(0.55,0.30,bri)*0.45;
        stoneBase=mix(stoneBase,vec3(0.08,0.17,0.05),mossAmount);
        vec3 mortarColor=vec3(0.055,0.085,0.05);
        vec3 finalColor=mix(stoneBase,mortarColor,inMortar);
        if(isRaining==1){finalColor*=0.70;finalColor=mix(finalColor,finalColor*1.5,(1.0-inMortar)*0.25);}
        return finalColor;
    }
    // Water
    if(matType==3){
        float n=hash(floor(FragPos.xz*6.0));
        float wave=sin(FragPos.x*2.1+time*1.8)*0.06+sin(FragPos.z*1.7+time*2.2)*0.04;
        vec3 wDay=mix(vec3(0.06,0.32,0.62),vec3(0.12,0.46,0.76),clamp(n+wave,0.0,1.0));
        vec3 wNight=mix(vec3(0.01,0.05,0.18),vec3(0.02,0.10,0.28),clamp(n+wave,0.0,1.0));
        return mix(wNight,wDay,dayFactor);
    }
    // Dirt
    if(matType==4){
        float n=vnoise(FragPos.xz*3.5),n2=vnoise(FragPos.xz*8.8+vec2(5.3,1.7));
        vec3 base=mix(vec3(0.34,0.23,0.10),vec3(0.44,0.31,0.16),n);
        base=mix(base,base*(0.88+0.22*n2),0.4);
        float rx=abs(FragPos.x),rut=smoothstep(0.5,1.2,rx)*(1.0-smoothstep(1.2,2.8,rx));
        base*=(1.0-0.10*rut);
        if(isRaining==1)base*=0.68;
        return base;
    }
    // Checkerboard
    if(matType==5){
        vec2 grid=floor(TexCoord*4.0);
        float pattern=mod(grid.x+grid.y,2.0);
        return mix(vec3(0.1),vec3(0.9),pattern)*objectColor;
    }
    // Procedural wood
    if(matType==6){
        float n=fBm(FragPos.xy*2.0+FragPos.zz*1.5);
        float dist=length(FragPos.xz)*1.5+n*0.8;
        float ring=fract(dist*3.0);
        ring=smoothstep(0.0,0.2,ring)-smoothstep(0.8,1.0,ring);
        return mix(vec3(0.25,0.10,0.05),vec3(0.70,0.45,0.15),ring)*objectColor;
    }
    // Textured UV
    if(matType==7){
        // Moon: blend two textures when u_moonBlend > 0
        vec3 c0=texture(tex0,TexCoord).rgb;
        vec3 c1=texture(tex1,TexCoord).rgb;
        vec3 texCol=mix(c0,c1,u_moonBlend);
        return texCol*objectColor;
    }
    // Triplanar
    if(matType==8){
        vec3 blend=abs(Normal);
        blend=max(blend,0.00001);
        blend/=(blend.x+blend.y+blend.z);
        float scale=0.5;
        vec3 xaxis=texture(tex0,FragPos.yz*scale).rgb;
        vec3 yaxis=texture(tex0,FragPos.xz*scale).rgb;
        vec3 zaxis=texture(tex0,FragPos.xy*scale).rgb;
        return (xaxis*blend.x+yaxis*blend.y+zaxis*blend.z)*objectColor;
    }
    float surf=vnoise(FragPos.xz*5.5+FragPos.y*3.3);
    return objectColor*(0.92+0.08*surf);
}

void main()
{
    vec3 color=getBaseColor();
    vec3 result;

    if(isEmissive==1)
    {
        result=(matType==7||matType==8)?color:objectColor;
    }
    else
    {
        vec3 norm=normalize(Normal);
        vec3 vDir=normalize(viewPos-FragPos);

        if(matType==2){
            float nx=vnoise(FragPos.xz*9.0+vec2(17.3,0.0))-0.5;
            float nz=vnoise(FragPos.xz*9.0+vec2(0.0,31.7))-0.5;
            norm=normalize(norm+vec3(nx,0.0,nz)*0.18);
        }
        if(matType==3){
            float wx=sin(FragPos.x*2.1+time*1.8)*0.12+sin(FragPos.z*1.7+time*2.2)*0.08;
            norm=normalize(norm+vec3(wx,0.0,wx*0.5));
        }

        // Ambient
        vec3 ambDay  =0.28*vec3(0.90,0.92,1.00);
        vec3 ambNight=0.12*vec3(0.20,0.30,0.65);
        vec3 ambient =mix(ambNight,ambDay,dayFactor);
        if(isRaining==1)ambient*=0.44;

        // Sun
        float sDiff=max(dot(norm,normalize(sunDir)),0.0);
        vec3 sunL=sDiff*sunColor*sunIntensity*dayFactor;
        if(isRaining==1)sunL*=0.30;
        vec3 sunRef=reflect(-normalize(sunDir),norm);
        float sSpec=pow(max(dot(vDir,sunRef),0.0),22.0);
        float specMul=(matType==1)?0.015:0.12;
        vec3 sunSpec=specMul*sSpec*sunColor*dayFactor;
        if(isRaining==1)sunSpec*=0.4;
        if(isRaining==1&&matType==2)sunSpec*=5.0;

        // Moon diffuse + rim
        float mDiff=max(dot(norm,normalize(moonDir)),0.0);
        vec3  moonL=mDiff*moonColor*(1.0-dayFactor)*0.55;
        vec3  rimL =moonColor*pow(max(1.0-dot(norm,vDir),0.0),3.0)*(1.0-dayFactor)*0.18;

        // Fire
        vec3 tF=firePos-FragPos;
        float fDist=length(tF);
        float fAtt=1.0/(0.25+0.07*fDist+0.035*fDist*fDist);
        float fDiff=max(dot(norm,normalize(tF)),0.0);
        vec3 fireL=fDiff*fireColor*fireIntensity*fAtt;
        vec3 fRef=reflect(-normalize(tF),norm);
        float fSpec=pow(max(dot(vDir,fRef),0.0),24.0);
        vec3 fireS=0.22*fSpec*fireColor*fireIntensity*fAtt;

        // Lamps
        vec3 lampL=vec3(0.0),lampSpec=vec3(0.0);
        vec3 lampC=vec3(1.0,0.86,0.48);
       for (int i = 0; i < numLamps && i < 12; i++)
    {
        vec3  tL   = lampPos[i] - FragPos;
        float lD   = length(tL);
        float lAtt = 1.0 / (0.45 + 0.12 * lD + 0.055 * lD * lD);
        float lDif = max(dot(norm, normalize(tL)), 0.0);
        lampL     += lDif * lampC * lampIntensity * lAtt;
        vec3  lRef = reflect(-normalize(tL), norm);
        float lSp  = pow(max(dot(vDir, lRef), 0.0), 30.0);
        float gm   = (isRaining == 1 && matType == 2) ? 8.0 : 0.5;
        lampSpec  += lSp * lampC * lampIntensity * lAtt * gm;
    }

    // ── AO hint ────────────────────────────────────────────────────────────
    float aoHint = 0.72 + 0.28 * clamp(norm.y, 0.0, 1.0);

        // ── Porch-style spotlight (fixed in world space) ───────────────────
        vec3 spotPos = vec3(-8.0, 7.0, 22.0);
        vec3 spotDir = normalize(vec3(0.12, -0.40, -1.0));
        vec3 Fspot = normalize(FragPos - spotPos);
        float spotEdge = smoothstep(0.90, 0.97, dot(Fspot, spotDir));
        float spotAtt = 1.0 / (1.0 + 0.07 * length(spotPos - FragPos));
        vec3 tToSpot = normalize(spotPos - FragPos);
        float spotDif = max(dot(norm, tToSpot), 0.0);
        vec3 spotL = spotEdge * spotAtt * spotDif * vec3(1.0, 0.92, 0.78) * 2.0;
        vec3 sRefS = reflect(-tToSpot, norm);
        float spotS = pow(max(dot(vDir, sRefS), 0.0), 28.0);
        vec3 spotSpec = 0.20 * spotEdge * spotAtt * spotS * vec3(1.0, 0.92, 0.78);
        // Car headlight (cone spotlight, two beams side by side)
vec3 carHL = vec3(0.0);
if (carHeadlightOn == 1) {
    for (int hi = 0; hi < 2; hi++) {
        // Side offset in world-space (perpendicular to light dir, approx X-axis)
        float side = (hi == 0) ? -0.34 : 0.34;
        vec3 hlPos = carHeadlightPos + vec3(side, 0.0, 0.0);

        vec3  toFrag   = FragPos - hlPos;
        float dist     = length(toFrag);
        vec3  Ldir     = normalize(toFrag);

        // Cone attenuation
        float cosA   = dot(Ldir, normalize(carHeadlightDir));
        float eps    = carHeadlightInner - carHeadlightOuter;
        float coneA  = clamp((cosA - carHeadlightOuter) / eps, 0.0, 1.0);
        coneA        = coneA * coneA;

        // Distance attenuation
        float att    = 1.0 / (1.0 + 0.18 * dist + 0.08 * dist * dist);

        // Diffuse + specular (Blinn-Phong)
        float diff   = max(dot(norm, -Ldir), 0.0);
        vec3  halfV  = normalize(-Ldir + vDir);
        float spec   = pow(max(dot(norm, halfV), 0.0), 48.0);

        carHL += carHeadlightColor
               * (diff * 1.0 + spec * 0.35)
               * att * coneA * 3.2;
    }
}
        vec3 ambTerm = ambient * color * aoHint * lightAmbOn;
        vec3 diffDir = (sunL + moonL) * color * aoHint * lightDiffOn * lightDirOn;
        vec3 rimTerm = rimL * lightDirOn * lightDiffOn;
        vec3 diffPoint = (fireL * color + lampL * color) * lightDiffOn * lightPointOn;
        vec3 specTerm = (sunSpec + fireS + lampSpec) * lightSpecOn;
        vec3 diffSpot = spotL * color * lightDiffOn * lightSpotOn;
        vec3 specSpot = spotSpec * lightSpecOn * lightSpotOn;

        vec3 carHLTerm = carHL * color * lightPointOn;
result = ambTerm + diffDir + rimTerm + diffPoint + specTerm + diffSpot + specSpot + carHLTerm;

        // ── Subtle desaturation (less cartoonish) ──────────────────────────────
        float lum = dot(result, vec3(0.299, 0.587, 0.114));
        result     = mix(result, vec3(lum), 0.10);
    }

    // ── Height-biased fog ──────────────────────────────────────────────────
    float fogDens = isRaining == 1 ? fogDensity * 1.8 : fogDensity; 
    if (isBackground == 1) {
        fogDens = fogDens * 0.45; // Increased from 0.15 to 0.45 so fog visible on horizon
    }
    
    float fd      = length(FragPos - viewPos);
    float hf      = 1.0 + 0.35 * clamp(1.0 - FragPos.y * 0.45, 0.0, 1.0);
    float fog     = exp(-fd * fogDens * hf);
    result        = mix(skyColor, result, clamp(fog, 0.0, 1.0));

    // ── Thunder Flash ───────────────────────────────────────────────────────
    result += vec3(thunderEffect * 0.45, thunderEffect * 0.48, thunderEffect * 0.52);

    // Get alpha if it's a texture, else 1.0
    float alpha = 1.0;
    if (matType == 7 || matType == 8) {
        alpha = texture(tex0, TexCoord).a;
    }
    
    FragColor = vec4(result, alpha);
}
