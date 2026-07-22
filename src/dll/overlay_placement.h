#pragma once

namespace tl2_overlay_placement {

struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

struct Placement {
    int x;
    int y;
    int width;
    int height;
};

inline int ClampInt(int value, int minimum, int maximum) {
    if (maximum < minimum) {
        return minimum;
    }
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline Placement ComputePlacement(
    const Rect& anchor,
    const Rect& work,
    int desiredWidth,
    int desiredHeight,
    int margin) {
    const int workWidth = work.right > work.left ? work.right - work.left : 1;
    const int workHeight = work.bottom > work.top ? work.bottom - work.top : 1;
    const int safeMargin = margin > 0 ? margin : 0;

    int maximumWidth = workWidth - safeMargin * 2;
    int maximumHeight = workHeight - safeMargin * 2;
    if (maximumWidth <= 0) maximumWidth = workWidth;
    if (maximumHeight <= 0) maximumHeight = workHeight;

    int width = desiredWidth > 0 ? desiredWidth : 1;
    int height = desiredHeight > 0 ? desiredHeight : 1;
    if (width > maximumWidth) width = maximumWidth;
    if (height > maximumHeight) height = maximumHeight;

    const bool anchorValid = anchor.right > anchor.left && anchor.bottom > anchor.top;
    const Rect reference = anchorValid ? anchor : work;
    int x = reference.left + ((reference.right - reference.left) - width) / 2;
    int y = reference.top + ((reference.bottom - reference.top) - height) / 2;

    int minimumX = work.left + safeMargin;
    int minimumY = work.top + safeMargin;
    int maximumX = work.right - safeMargin - width;
    int maximumY = work.bottom - safeMargin - height;
    if (maximumX < minimumX) {
        minimumX = work.left;
        maximumX = work.right - width;
    }
    if (maximumY < minimumY) {
        minimumY = work.top;
        maximumY = work.bottom - height;
    }

    x = ClampInt(x, minimumX, maximumX);
    y = ClampInt(y, minimumY, maximumY);
    return Placement{x, y, width, height};
}

inline int ComputeAutoTextScalePercent(int clientHeight) {
    const int safeHeight = clientHeight > 0 ? clientHeight : 1080;
    int percent = (safeHeight * 100) / 1080;
    if (percent < 88) percent = 88;
    if (percent > 170) percent = 170;
    return percent;
}

inline Placement ComputeCoverPlacement(
    const Rect& anchor,
    const Rect& bounds) {
    const int boundsWidth = bounds.right > bounds.left ? bounds.right - bounds.left : 1;
    const int boundsHeight = bounds.bottom > bounds.top ? bounds.bottom - bounds.top : 1;

    int left = anchor.right > anchor.left ? anchor.left : bounds.left;
    int top = anchor.bottom > anchor.top ? anchor.top : bounds.top;
    int right = anchor.right > anchor.left ? anchor.right : bounds.right;
    int bottom = anchor.bottom > anchor.top ? anchor.bottom : bounds.bottom;

    if (left < bounds.left) left = bounds.left;
    if (top < bounds.top) top = bounds.top;
    if (right > bounds.right) right = bounds.right;
    if (bottom > bounds.bottom) bottom = bounds.bottom;

    if (right <= left) {
        left = bounds.left;
        right = bounds.right;
    }
    if (bottom <= top) {
        top = bounds.top;
        bottom = bounds.bottom;
    }

    return Placement{
        left,
        top,
        right - left > 0 ? right - left : boundsWidth,
        bottom - top > 0 ? bottom - top : boundsHeight};
}

} // namespace tl2_overlay_placement
