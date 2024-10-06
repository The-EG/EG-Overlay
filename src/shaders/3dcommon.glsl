#define BILLBOARD  (1u)
//#define CENTERFADE (1u << 1)

// Calculate the alpha based on distance given near and far thresholds.
// Distances less than near will be 1.0, more than far will be 0.0, and linear
// interpolated in between.
float distance_fade_alpha(float near, float far, float dist) {
    if (near < 0.0 || near > far || dist < near) return 1.0;

    if (dist > far) return 0.0;

    return 1.0 - ((dist - near) / (far - near));
}

#ifdef FRAGMENT_SHADER

// Calculate the alpha based on the location on screen. Locations within a given
// radius of a center point will be nearly transparent (0.05), those outside
// of radius r + radius fr will be 1.0. In between will be linearly interpolated.
float fade_region_alpha(vec2 center, float r, float fr) {
    float dist = distance(center, gl_FragCoord.xy);
    if (dist <= r) return 0.05;

    if (dist < (r + fr)) {
        float fade = (dist - r) / fr;
        return max(fade, 0.05);
    }

    return 1.0;
}

// Discard this fragment if it's within the area where the minimap is
// We don't need the map width because it's always against the right edge of
// the screen
void discard_if_in_map(float mapleft, float mapbottom, float mapheight) {
    if (gl_FragCoord.x >= mapleft &&
        gl_FragCoord.y >= mapbottom && gl_FragCoord.y <= (mapbottom + mapheight)
    ) {
        discard;
    }
}

#endif
