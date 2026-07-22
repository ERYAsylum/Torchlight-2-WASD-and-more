#include "../dll/camera_math.h"
#include "../dll/overlay_placement.h"

#include <stdio.h>

namespace {

constexpr float kTolerance = 0.00001f;

bool Near(float actual, float expected) {
    return fabsf(actual - expected) <= kTolerance;
}

bool CheckForward(
    const char* name,
    const tl2_camera_math::Quaternion& orientation,
    float expectedX,
    float expectedZ) {
    float x = 0.0f;
    float z = 0.0f;
    if (!tl2_camera_math::TryGetForwardXZ(orientation, &x, &z) ||
        !Near(x, expectedX) || !Near(z, expectedZ)) {
        printf("FAIL %s: forwardXZ=(%.6f,%.6f) expected=(%.6f,%.6f)\n",
            name, x, z, expectedX, expectedZ);
        return false;
    }
    return true;
}


bool CheckPlacement(
    const char* name,
    const tl2_overlay_placement::Rect& anchor,
    const tl2_overlay_placement::Rect& work,
    int expectedX,
    int expectedY,
    int expectedWidth,
    int expectedHeight) {
    const tl2_overlay_placement::Placement placement =
        tl2_overlay_placement::ComputePlacement(anchor, work, 820, 560, 16);
    if (placement.x != expectedX || placement.y != expectedY ||
        placement.width != expectedWidth || placement.height != expectedHeight) {
        printf(
            "FAIL %s: placement=(%d,%d,%d,%d) expected=(%d,%d,%d,%d)\n",
            name,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            expectedX,
            expectedY,
            expectedWidth,
            expectedHeight);
        return false;
    }
    return true;
}


bool CheckCoverPlacement(
    const char* name,
    const tl2_overlay_placement::Rect& anchor,
    const tl2_overlay_placement::Rect& bounds,
    int expectedX,
    int expectedY,
    int expectedWidth,
    int expectedHeight) {
    const tl2_overlay_placement::Placement placement =
        tl2_overlay_placement::ComputeCoverPlacement(anchor, bounds);
    if (placement.x != expectedX || placement.y != expectedY ||
        placement.width != expectedWidth || placement.height != expectedHeight) {
        printf(
            "FAIL %s: cover=(%d,%d,%d,%d) expected=(%d,%d,%d,%d)\n",
            name,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            expectedX,
            expectedY,
            expectedWidth,
            expectedHeight);
        return false;
    }
    return true;
}

} // namespace

int main() {
    const tl2_camera_math::Quaternion referenceMinus45{
        0.351360f, -0.151633f, 0.848259f, 0.366074f};
    const tl2_camera_math::Quaternion referenceMinus20{
        0.159435f, -0.068806f, 0.904200f, 0.390216f};

    bool passed = true;
    passed &= CheckForward("reference -45", referenceMinus45, -0.707107f, 0.707107f);
    passed &= CheckForward("reference -20", referenceMinus20, -0.342020f, 0.939693f);

    float forwardX = 0.0f;
    float forwardZ = 0.0f;
    float movementX = 0.0f;
    float movementZ = 0.0f;
    float gameplayUpX = 0.0f;
    float gameplayUpZ = 0.0f;
    passed &= tl2_camera_math::TryGetForwardXZ(referenceMinus20, &forwardX, &forwardZ);
    passed &= tl2_camera_math::TryComposeMovementVector(
        forwardX, forwardZ, 1, 0, &movementX, &movementZ);
    passed &= Near(movementX, -0.342020f) && Near(movementZ, 0.939693f);
    passed &= tl2_camera_math::TryComposeMovementVector(
        forwardX, forwardZ, 0, 1, &movementX, &movementZ);
    passed &= Near(movementX, -0.939693f) && Near(movementZ, -0.342020f);
    passed &= tl2_camera_math::TryComposeMovementVector(
        forwardX, forwardZ, 1, 1, &movementX, &movementZ);
    passed &= Near(movementX * movementX + movementZ * movementZ, 1.0f);

    passed &= tl2_camera_math::TrySelectIsometricGameplayBasis(
        referenceMinus45, &gameplayUpX, &gameplayUpZ);
    passed &= Near(gameplayUpX, -0.707107f) && Near(gameplayUpZ, 0.707107f);
    passed &= tl2_camera_math::TrySelectIsometricGameplayBasis(
        referenceMinus20, &gameplayUpX, &gameplayUpZ);
    passed &= Near(gameplayUpX * gameplayUpX + gameplayUpZ * gameplayUpZ, 1.0f);

    const tl2_camera_math::Quaternion verticalLook{
        0.70710678f, -0.70710678f, 0.0f, 0.0f};
    passed &= !tl2_camera_math::TryGetForwardXZ(verticalLook, &forwardX, &forwardZ);

    passed &= CheckPlacement(
        "1920x1080 fullscreen centre",
        tl2_overlay_placement::Rect{0, 0, 1920, 1080},
        tl2_overlay_placement::Rect{0, 0, 1920, 1040},
        550, 260, 820, 560);
    passed &= CheckPlacement(
        "1280x720 window centre",
        tl2_overlay_placement::Rect{100, 100, 1380, 820},
        tl2_overlay_placement::Rect{0, 0, 1920, 1040},
        330, 180, 820, 560);
    passed &= CheckPlacement(
        "secondary monitor centre",
        tl2_overlay_placement::Rect{1920, 0, 4480, 1440},
        tl2_overlay_placement::Rect{1920, 0, 4480, 1400},
        2790, 440, 820, 560);
    passed &= CheckPlacement(
        "small work area fitted",
        tl2_overlay_placement::Rect{0, 0, 800, 600},
        tl2_overlay_placement::Rect{0, 0, 800, 600},
        16, 20, 768, 560);
    passed &= CheckPlacement(
        "off-screen anchor clamped",
        tl2_overlay_placement::Rect{-500, -400, 500, 400},
        tl2_overlay_placement::Rect{0, 0, 1920, 1040},
        16, 16, 820, 560);

    passed &= CheckCoverPlacement(
        "fullscreen client cover",
        tl2_overlay_placement::Rect{0, 0, 1920, 1080},
        tl2_overlay_placement::Rect{0, 0, 1920, 1080},
        0, 0, 1920, 1080);
    passed &= CheckCoverPlacement(
        "windowed client cover",
        tl2_overlay_placement::Rect{120, 80, 1720, 980},
        tl2_overlay_placement::Rect{0, 0, 1920, 1080},
        120, 80, 1600, 900);
    passed &= CheckCoverPlacement(
        "partially offscreen client clipped",
        tl2_overlay_placement::Rect{-100, 40, 1500, 940},
        tl2_overlay_placement::Rect{0, 0, 1920, 1080},
        0, 40, 1500, 900);
    passed &= tl2_overlay_placement::ComputeAutoTextScalePercent(720) == 88;
    passed &= tl2_overlay_placement::ComputeAutoTextScalePercent(1080) == 100;
    passed &= tl2_overlay_placement::ComputeAutoTextScalePercent(1440) == 133;
    passed &= tl2_overlay_placement::ComputeAutoTextScalePercent(2160) == 170;

    if (!passed) {
        puts("Core movement and overlay placement tests failed.");
        return 1;
    }

    puts("Core movement and overlay placement tests passed.");
    return 0;
}
