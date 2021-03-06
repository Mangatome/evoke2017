//! FRAGMENT

uniform float _u[UNIFORM_COUNT];

#define iResolution vec3(_u[4], _u[5], 0.)
#define iGlobalTime _u[0]

//-------- stoy below

mat2 rot( in float a ) {
	float c = cos(a),
        s = sin(a);
    return mat2(c, s, -s, c);
}

float sdStrip( in vec2 uv, in float angle, in float size ) {
	vec2 uvS = 10. * rot(angle) * uv;
    return max(uv.x - uvS.x - size, -uv.x + uvS.x - size);
}

float sdEllipsoid( in vec2 p, in vec2 r )
{
    return (length( p/r ) - 1.0) * min(r.x,r.y);
}

float sdRay(in vec2 p, in float width, in float angle, in float lgbth) {
	float a = atan(p.y,p.x)/6.28318531;
	float diff = abs(angle - a);
	float ray = smoothstep(diff, diff + width, width);
	return ray * smoothstep(length(p), length(p) + width * 2, lgbth);
}

float rd(vec2 p, float x) {
    p.x=mod(p.x,x);
    return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453123);
}

float rand(vec2 p){
    return fract(sin(dot(p ,vec2(12.9898,78.233))) * 43758.5453);
}

float noise(vec2 uv, float x) {
    vec2 iuv=floor(uv),
        fuv=smoothstep(0.,1.,fract(uv)),
        oi=vec2(0.,1.);
    float n00=rd(iuv,x),
        n01=rd(iuv+oi.xy,x),
        n10=rd(iuv+oi.yx,x),
        n11=rd(iuv+oi.yy,x);
    return mix(
        mix(n00,n10,fuv.x),
        mix(n01,n11,fuv.x),
        fuv.y);
}

vec3 hsv(vec3 c) {
    vec4 k=vec4(1.,2./3.,1./3.,3.);
    vec3 p=abs(fract(c.xxx+k.xyz)*4.-k.www);
    return c.z*mix(k.xxx,clamp(p-k.xxx,0.,1.),c.y);
}

float swipe( in float period, in float bmin, in float bmax, float offset ) {
    float s = 2. * max(mod(iGlobalTime + offset, 2. * period) - period, 0.) - period;
    return (((s / period) + 1.) * .5 * (bmax - bmin)) + bmin;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	// Constants
	float PI = 3.14159265359;
	float it = iGlobalTime;
	vec2 uv = fragCoord.xy / iResolution.xy;
	float E = 1. / iResolution.y;

	// Tweak here!
	float shakeAmpSpeed = 20.;
	float headSize = 1.5;
	float growHFactor = .025;
	float fluffynessH = .2;
	float fluffynessLR = .02;
	float fluffyPeriod = .5;
	float swipePeriod = .43;
	float swipeOffset = .6;
	float mustachePeriod = .5;
	float mustacheAmplitude = 0.005;
	float mustacheLength = .3;
	vec2 mustachePan = vec2(0., .13);
	vec3 mustacheAngles = vec3(-.02, 0, .02);

	// Colors
	vec3 black = vec3(0., 0., 0.);
	vec3 white = vec3(1., 1., 1.);
	vec3 pink = vec3(1., 0.58, 0.75);
	vec3 pinker = vec3(1., .2, .5);

    // aspect x
    uv -= 0.5;
    uv.x *= iResolution.x / iResolution.y;

    // Timeline
    // 0 -> 13.6 : appear
    // 13.6 -> 24 : bass
    // 24 -> 39.4 : happy
    // 39.4 -> 44.6 : disappear
    // 49.2 : appear 2
    // 52.8 : disappear 2
    float tAppearH = smoothstep(0., 14., it);
    float tshakeH = smoothstep(0., 5., it) * smoothstep(13.6, 8., it);
    float tFluffyH = smoothstep(4., 6., it);
    float tRays = smoothstep(13., 17., it) * smoothstep(24., 20., it);
    float tSwipes = step(13.4, it) * step(it, 20);
    float tOffsetH = smoothstep(22.3, 24., it);
    float tGrowH = smoothstep(24., 24.1, it);
    float tReduceH = smoothstep(24.1, 24.6, it);
    float tAppearL = smoothstep(25.4, 25.9, it);
    float tAppearR = smoothstep(27., 27.5, it);
    float tAppearML1 = smoothstep(30.4, 30.9, it);
    float tAppearMR1 = smoothstep(32.1, 32.6, it);
    float tAppearML2 = smoothstep(33.8, 34.3, it);
    float tAppearMR2 = smoothstep(35.5, 36., it);
    float tAppearML3 = smoothstep(37.3, 37.7, it);
    float tAppearMR3 = smoothstep(39., 39.5, it);
    float tPink = smoothstep(27.6, 29.1, it);
    float tExplode = smoothstep(39., 46.5, it);
    float tShowRaby = smoothstep(49., 49.5, it);
    float tHideRaby = smoothstep(52.75, 53.25, it);
    float tFadeOut = smoothstep(55., 56., it);
    //float tAppearH = smoothstep(0., 5., it);
    //float tshakeH = smoothstep(0., 5., it) * smoothstep(28., 20., it);
    //float tFluffyH = smoothstep(4., 6., it);
    //float tRays = smoothstep(15., 20., it) * smoothstep(35., 33., it);
    //float tSwipes = step(15., it) * step(it, 28.5);
    //float tOffsetH = smoothstep(20., 28., it);
    //float tAppearL = smoothstep(28.25, 28.75, it);
    //float tAppearR = smoothstep(29.25, 29.75, it);
    //float tPink = smoothstep(31., 32., it);
    //float tExplode = smoothstep(33., 35., it);
    //float tShowRaby = smoothstep(36., 37., it);
    //float tHideRaby = smoothstep(38., 39., it);
    //float tFadeOut = smoothstep(39.5, 40., it);

    // Scene management
    float scene2 = step(1, tExplode);
    float scene1 = 1 - scene2;

    vec3 rabbitColor = white * scene1 + pink * scene2;
    vec3 earColor = pink * scene1 + white * scene2;

    // Zoom scene
    uv *= 1 - tExplode * 1 + scene2;

    // Pan Daddy
    vec2 rabbitPan = vec2(0., 1.) * tOffsetH;

    // Pan Rabby
    uv += scene2 * mix(vec2(.3, 1.), vec2(.2, .55), tShowRaby - tHideRaby);
    uv = uv * scene1 + uv * rot(PI * -.1) * scene2;

    // Background color
    vec3 c = mix(black, pinker, tPink);
    c = mix(c, white, scene2);

    // Rays
    float nr = 400.;
    vec2 uvr = uv + rabbitPan / 10;
    float ruvr = rand(uvr * it);
    float ang=atan(uvr.y,uvr.x)/6.28318531*nr+(ruvr-.5)*it;
    float ns=noise(vec2(ang,it*ruvr),nr);
    vec3 cex = c + .1
    		* ns/length(uvr)
			* hsv(vec3(1.,.5,1.2))
			* step(length(uvr), mix(.35, .45, ruvr));
    c = mix(c, cex, tRays);

    // Ear size
    vec2 rext = vec2(.3, .8);
    vec2 rint = rext - .2;

    // Fluffy management
    float fluffyL = (1 + sin(it * fluffyPeriod - fluffyPeriod * .5)) * fluffynessLR;
    float fluffyH = (1 + sin(it * fluffyPeriod)) * fluffynessH;
    float fluffyR = (1 + sin(it * fluffyPeriod + fluffyPeriod * .5)) * fluffynessLR;

    // right ear - ellipsoid inner + outer
    float angle = 0.5 + (0.035 + 0.05 * cos(it + 1.));
    vec2 uvx = rot(angle) * uv;
    uvx *= 4.0;
    uvx += vec2(-0.2, -0.7 * tAppearR);
    float d = sdEllipsoid(uvx, rint * tAppearR);
    float r = step(d, 0.);
    c = mix(c, earColor, r * tAppearR);
    d = max(sdEllipsoid(uvx, rext * tAppearR), -d);
    r = smoothstep(.01 + fluffyL, .0, d);
    c = mix(c, rabbitColor, r);

    // left ear - ellipsoid inner + outer
    angle = 0.37 + (0.05 + 0.05 * sin(it * 1.05));
    uvx = uv * rot(angle);
    uvx *= 4.0;
    uvx += vec2(0.2, -0.7 * tAppearL);
    d = sdEllipsoid(uvx, rint * tAppearL);
    r = step(d, 0.);
    c = mix(c, earColor, r * tAppearL);
    d = max(sdEllipsoid(uvx, rext * tAppearL), -d);
    r = smoothstep(.01 + fluffyR, .0, d);
    c = mix(c, rabbitColor, r);

    // head - circle
    uvx = uv * 10.0 / tAppearH;
    uvx += rabbitPan;
    uvx *= rot(it * 4);
    float amplitude = .1 * tshakeH;
    d = sdEllipsoid(uvx, headSize * vec2(
    		1 + amplitude * cos(it * shakeAmpSpeed),
			1 + amplitude * sin(it * shakeAmpSpeed)
    ) * (1 + (tGrowH - tReduceH) * growHFactor));
    r = smoothstep(.32 + fluffyH * tFluffyH + .05 / tAppearH, .30, d); // [0., 1.]
    c = mix(c, rabbitColor, r);

    // Mustaches
    float ms = sin(it / mustachePeriod) * mustacheAmplitude;
    vec2 uvrot = uv * rot(PI);
    d = sdRay(uv + mustachePan, .005, mustacheAngles.x + ms, mustacheLength * tAppearMR3);
    c = mix(c, rabbitColor, d);
    d = sdRay(uv + mustachePan, .005, mustacheAngles.y + ms, mustacheLength * tAppearMR2);
    c = mix(c, rabbitColor, d);
    d = sdRay(uv + mustachePan, .005, mustacheAngles.z + ms, mustacheLength * tAppearMR1);
    c = mix(c, rabbitColor, d);
    d = sdRay(uvrot - mustachePan, .005, mustacheAngles.x + ms, mustacheLength * tAppearML1);
    c = mix(c, rabbitColor, d);
    d = sdRay(uvrot - mustachePan, .005, mustacheAngles.y + ms, mustacheLength * tAppearML2);
    c = mix(c, rabbitColor, d);
    d = sdRay(uvrot - mustachePan, .005, mustacheAngles.z + ms, mustacheLength * tAppearML3);
    c = mix(c, rabbitColor, d);

    // Swipe
    uvx = uv + swipe(swipePeriod, 2., -2., swipeOffset);
    d = sdStrip(uvx, 0.3, 1.);
    r = smoothstep(E + 0.05, E, d);
    vec3 invertColor = (1. - c) * pink;
    c = mix(c, mix(c, invertColor, r), tSwipes);

    // Fade out
    c = mix(c, black, tFadeOut);

    // return
	fragColor = vec4(c, 1.);
}

//-------- stoy above

void main(void) {
	vec4 color;
	mainImage(color, gl_FragCoord.xy);

	color.w = 1.0;
	gl_FragColor = color;
}
