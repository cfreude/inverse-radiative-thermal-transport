#version 460 

layout(location = 0) in vec2 in_uv;

layout(push_constant) uniform PushConstant {
    layout(offset = 16) vec3 color;
};

layout(location = 0) out vec4 fragColor;

void main() {
    // https://gamedev.stackexchange.com/questions/141264/an-efficient-way-for-generating-smooth-circle
    float THICKNESS = 1;
    // Compute this fragment's distance from the center of the circle in uv space.
    float radius = length(in_uv);
    // Convert to a signed distance from the outline we want to draw.
    // +ve: outside the disc, -ve: inside the disc.
    float signedDistance = radius - 1.0f;
    // Use screenspace derivatives to compute how quickly this value changes
    // as we move one pixel left or right / up or down:
    vec2 gradient = vec2(dFdx(signedDistance), dFdy(signedDistance));
    // Compute (approximately) how far we are from the line in pixel coordinates.
    float rangeFromLine = abs(signedDistance/length(gradient));
    // Combine with our desired line thickness (in pixels) to get an opacity.
    float alpha = clamp(THICKNESS - rangeFromLine, 0.0f, 1.0f);

    // dot in the middle
    // float signedDistance2 = radius;
    // vec2 gradient2 = vec2(dFdx(signedDistance2), dFdy(signedDistance2));
    // float rangeFromLine2 = abs(signedDistance2/length(gradient2));
    // if(clamp(THICKNESS - rangeFromLine2, 0.0f, 1.0f) > 0.1f) lineWeight = 1;

    fragColor = vec4(color, alpha);
}