#pragma once

#include <float.h>
#include <math.h>

namespace tl2_camera_math {

struct Quaternion {
    float w;
    float x;
    float y;
    float z;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

inline bool IsFinite(float value) {
#if defined(_MSC_VER)
    return _finite(static_cast<double>(value)) != 0;
#else
    return isfinite(value) != 0;
#endif
}

inline Vector3 Cross(const Vector3& left, const Vector3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

inline Vector3 Add(const Vector3& left, const Vector3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline bool TryGetForwardXZ(
    const Quaternion& orientation,
    float* forwardXOut,
    float* forwardZOut) {
    if (forwardXOut == nullptr || forwardZOut == nullptr) {
        return false;
    }

    const float values[4] = {
        orientation.w,
        orientation.x,
        orientation.y,
        orientation.z};
    for (float value : values) {
        if (!IsFinite(value)) {
            return false;
        }
    }

    const float quaternionLengthSquared =
        orientation.w * orientation.w +
        orientation.x * orientation.x +
        orientation.y * orientation.y +
        orientation.z * orientation.z;
    if (!IsFinite(quaternionLengthSquared) || quaternionLengthSquared <= 0.000001f) {
        return false;
    }

    const float inverseQuaternionLength = 1.0f / sqrtf(quaternionLengthSquared);
    const Quaternion normalized{
        orientation.w * inverseQuaternionLength,
        orientation.x * inverseQuaternionLength,
        orientation.y * inverseQuaternionLength,
        orientation.z * inverseQuaternionLength};
    const Vector3 quaternionVector{normalized.x, normalized.y, normalized.z};
    const Vector3 negativeUnitZ{0.0f, 0.0f, -1.0f};
    const Vector3 twiceCross = Scale(Cross(quaternionVector, negativeUnitZ), 2.0f);
    const Vector3 look = Add(
        negativeUnitZ,
        Add(Scale(twiceCross, normalized.w), Cross(quaternionVector, twiceCross)));

    const float projectionLengthSquared = look.x * look.x + look.z * look.z;
    if (!IsFinite(projectionLengthSquared) || projectionLengthSquared <= 0.000001f) {
        return false;
    }

    const float inverseProjectionLength = 1.0f / sqrtf(projectionLengthSquared);
    *forwardXOut = look.x * inverseProjectionLength;
    *forwardZOut = look.z * inverseProjectionLength;
    return IsFinite(*forwardXOut) && IsFinite(*forwardZOut);
}

inline bool TryRotateVector(
    const Quaternion& orientation,
    const Vector3& vector,
    Vector3* rotatedOut) {
    if (rotatedOut == nullptr) {
        return false;
    }

    const float values[4] = {
        orientation.w,
        orientation.x,
        orientation.y,
        orientation.z};
    for (float value : values) {
        if (!IsFinite(value)) {
            return false;
        }
    }

    const float quaternionLengthSquared =
        orientation.w * orientation.w +
        orientation.x * orientation.x +
        orientation.y * orientation.y +
        orientation.z * orientation.z;
    if (!IsFinite(quaternionLengthSquared) || quaternionLengthSquared <= 0.000001f) {
        return false;
    }

    const float inverseQuaternionLength = 1.0f / sqrtf(quaternionLengthSquared);
    const Quaternion normalized{
        orientation.w * inverseQuaternionLength,
        orientation.x * inverseQuaternionLength,
        orientation.y * inverseQuaternionLength,
        orientation.z * inverseQuaternionLength};
    const Vector3 quaternionVector{normalized.x, normalized.y, normalized.z};
    const Vector3 twiceCross = Scale(Cross(quaternionVector, vector), 2.0f);
    *rotatedOut = Add(
        vector,
        Add(Scale(twiceCross, normalized.w), Cross(quaternionVector, twiceCross)));
    return IsFinite(rotatedOut->x) && IsFinite(rotatedOut->y) && IsFinite(rotatedOut->z);
}

inline bool TryProjectXZ(const Vector3& vector, float* xOut, float* zOut) {
    if (xOut == nullptr || zOut == nullptr) {
        return false;
    }

    const float lengthSquared = vector.x * vector.x + vector.z * vector.z;
    if (!IsFinite(lengthSquared) || lengthSquared <= 0.000001f) {
        return false;
    }

    const float inverseLength = 1.0f / sqrtf(lengthSquared);
    *xOut = vector.x * inverseLength;
    *zOut = vector.z * inverseLength;
    return IsFinite(*xOut) && IsFinite(*zOut);
}

inline bool TrySelectIsometricGameplayBasis(
    const Quaternion& orientation,
    float* upXOut,
    float* upZOut) {
    if (upXOut == nullptr || upZOut == nullptr) {
        return false;
    }

    Vector3 cameraUp{};
    Vector3 cameraRight{};
    if (!TryRotateVector(orientation, {0.0f, 1.0f, 0.0f}, &cameraUp) ||
        !TryRotateVector(orientation, {1.0f, 0.0f, 0.0f}, &cameraRight)) {
        return false;
    }

    float screenUpX = 0.0f;
    float screenUpZ = 0.0f;
    float screenRightX = 0.0f;
    float screenRightZ = 0.0f;
    if (!TryProjectXZ(cameraUp, &screenUpX, &screenUpZ) ||
        !TryProjectXZ(cameraRight, &screenRightX, &screenRightZ)) {
        return false;
    }

    constexpr float kInvSqrt2 = 0.7071067811865475f;
    const float candidates[4][2] = {
        {-kInvSqrt2, kInvSqrt2},
        {kInvSqrt2, kInvSqrt2},
        {kInvSqrt2, -kInvSqrt2},
        {-kInvSqrt2, -kInvSqrt2}};

    float bestScore = -FLT_MAX;
    int bestIndex = -1;
    for (int i = 0; i < 4; ++i) {
        const float upX = candidates[i][0];
        const float upZ = candidates[i][1];
        const float rightX = -upZ;
        const float rightZ = upX;
        const float score =
            upX * screenUpX +
            upZ * screenUpZ +
            rightX * screenRightX +
            rightZ * screenRightZ;
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestIndex < 0 || !IsFinite(bestScore)) {
        return false;
    }

    *upXOut = candidates[bestIndex][0];
    *upZOut = candidates[bestIndex][1];
    return true;
}

inline bool TryComposeMovementVector(
    float forwardX,
    float forwardZ,
    int forwardAxis,
    int rightAxis,
    float* movementXOut,
    float* movementZOut) {
    if (movementXOut == nullptr || movementZOut == nullptr ||
        !IsFinite(forwardX) || !IsFinite(forwardZ)) {
        return false;
    }

    const float rightX = -forwardZ;
    const float rightZ = forwardX;
    const float movementX =
        forwardX * static_cast<float>(forwardAxis) +
        rightX * static_cast<float>(rightAxis);
    const float movementZ =
        forwardZ * static_cast<float>(forwardAxis) +
        rightZ * static_cast<float>(rightAxis);
    const float movementLengthSquared = movementX * movementX + movementZ * movementZ;
    if (!IsFinite(movementLengthSquared) || movementLengthSquared <= 0.000001f) {
        return false;
    }

    const float inverseMovementLength = 1.0f / sqrtf(movementLengthSquared);
    *movementXOut = movementX * inverseMovementLength;
    *movementZOut = movementZ * inverseMovementLength;
    return IsFinite(*movementXOut) && IsFinite(*movementZOut);
}

} // namespace tl2_camera_math
