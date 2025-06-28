// EG-Overlay
// Copyright (c) 2025 Taylor Talkington
// SPDX-License-Identifier: MIT
#pragma once
#define BILLBOARD (1u)

// Calculate the alpha based on distance given near and far thresholds.
// Distances less than near will be 1.0, more than far will be 0.0, and linear
// interpolated in between.
float distance_fade_alpha(float near, float far, float dist) {
    if (near < 0.0 || near > far || dist < near) return 1.0;

    if (dist > far) return 0.0;

    return 1.0 - ((dist - near) / (far - near));
}

#ifdef PIXEL_SHADER

// Discard this fragment if it's within the area where the minimap is
// We don't need the map width because it's always against the right edge of
// the screen
void discard_if_in_map(float4 pixelcoord, float mapleft, float maptop, float mapheight) { 
    if (pixelcoord.x >= mapleft &&
        pixelcoord.y >= maptop && pixelcoord.y <= (maptop + mapheight)
    ) {
        discard;
    }
}

#endif
