// Builtins: iTime, iResolution (px), iMouse (px, origin bottom-left),
//           iGrid (cols,rows), iPaused, iFrame, iSeed.
// You implement this:
float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }

void mainImage(out vec4 fragColor, in vec2 fragCoord, in ivec2 cell, in vec2 cellUV)
{
    vec2 uv = cellUV * 2.0 - 1.0;
    float t = iTime + 0.3*float(iFrame);
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    float phase = hash(vec2(cell))*6.28318;

    float wave = sin(12.0*r - 8.0*(a + t) + phase);
    float mask = smoothstep(0.03, 0.0, abs(wave)*0.5);
    vec3 base = vec3(0.08,0.10,0.15);
    vec3 glow = vec3(0.2,0.7,1.0) * mask;
    fragColor = vec4(base + glow, 1.0);
}
