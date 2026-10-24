#version 450
// Draws the game's picture on the screen, with the display filter and scanlines the settings choose
// (s360_video.c sets p). The sampler is linear for the bilinear filters and point for sharp pixels.
//
// Sharp bilinear: each texel is a flat square and only its last screen pixel at each edge blends into the next, so
// pixels stay crisp at any size and the edges move smoothly instead of jumping a whole screen pixel. shape.xy is the
// screen pixels a texel covers; 1 there is plain bilinear.
//
// Scanlines: each line of the console's picture is drawn as a beam of light whose height grows with its brightness,
// as on a tube television, sampled three times across each screen pixel so the lines don't shimmer at 720 lines.
layout(location=0) in vec2 v_uv;
layout(set=0,binding=0) uniform sampler2D image;
layout(push_constant) uniform Params {
  vec4 tex;    // texture width and height in texels, and their reciprocals
  vec4 shape;  // screen pixels a texel covers (x, y), scanline strength 0 to 1, texels per console line
  vec4 beam;   // a third of a screen pixel in console lines, the brightness the beam gives back, unused, unused
} p;
layout(location=0) out vec4 colour;

void main() {
  vec2 texel = v_uv * p.tex.xy;
  vec2 base = floor(texel);
  vec2 centre = texel - base - 0.5;
  vec2 flat_part = 0.5 - 0.5 / p.shape.xy;
  vec2 offset = (centre - min(max(centre, -flat_part), flat_part)) * p.shape.xy + 0.5;  // clamp: xost takes constant bounds only
  vec3 rgb = texture(image, (base + offset) * p.tex.zw).rgb;

  // Each value is assigned once: xost refuses a local stored twice.
  float line = texel.y / p.shape.w;
  float width = mix(0.30, 0.48, dot(rgb, vec3(0.299, 0.587, 0.114)));
  // The beam, a gaussian exp(-2 d^2) of the distance from the line's middle in beam widths, at three points across the
  // screen pixel. Written out three times: xost has no function calls.
  float d0 = (fract(line - p.beam.x) - 0.5) / width;
  float d1 = (fract(line) - 0.5) / width;
  float d2 = (fract(line + p.beam.x) - 0.5) / width;
  float weight = (exp2(-2.88539 * d0 * d0) + exp2(-2.88539 * d1 * d1) + exp2(-2.88539 * d2 * d2)) * (1.0 / 3.0);
  vec3 lit = rgb * mix(1.0, weight * p.beam.y, p.shape.z);
  colour = vec4(min(lit, vec3(1.0)), 1.0);
}
