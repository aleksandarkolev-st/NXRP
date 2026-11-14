#pragma once

#include "MazePosition.h"
#include "MazeSize.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <utility>
#include <vector>

constexpr float refugeScale = 1.6f;
constexpr float refugeUpperFloor = 1.9f;

// Compact east-side stair. The landing joins the gallery at the north end;
// its open stairwell cannot be entered sideways from the gallery.
inline float refugeFloorHeight(float x, float z, float previous,
    const std::vector<MazePosition>& houses) noexcept
{
    for (const auto house : houses)
    {
        const float dx = (x - (house.col + .5f)) / refugeScale, dz = (z - (house.row + .5f)) / refugeScale;
        if (std::abs(dx) > 1.27f || std::abs(dz) > 1.27f) continue;
        if (dx > .65f && dz >= .30f) return refugeUpperFloor * std::clamp((dz - .30f) / .85f, 0.f, 1.f);
        if (previous > refugeUpperFloor - .12f) return refugeUpperFloor;
        return 0.f;
    }
    return 0.f;
}

// The 5x5 cabin footprint defines shelter geometry, not monster immunity.
inline bool insideSafehouse(MazePosition cell, MazePosition center) noexcept
{
    return std::abs(cell.row - center.row) <= 2 && std::abs(cell.col - center.col) <= 2;
}

inline int safehouseAt(MazePosition cell, const std::vector<MazePosition>& houses) noexcept
{
    for (std::size_t i = 0; i < houses.size(); ++i)
        if (insideSafehouse(cell, houses[i])) return static_cast<int>(i);
    return -1;
}

// Cabin wall crossings must use the centre doorway on each side.
inline bool refugeEdgeAllowed(MazePosition from, MazePosition to,
    const std::vector<MazePosition>& houses) noexcept
{
    for (const auto house : houses)
    {
        if (insideSafehouse(from, house) == insideSafehouse(to, house)) continue;
        if (from.row != to.row && from.col != house.col) return false;
        if (from.col != to.col && from.row != house.row) return false;
    }
    return true;
}

// Geometry shared by the renderer, collision and interaction code. Side 0..3
// is -X, +X, -Z, +Z. A closed door fills the middle of that side; the hinge
// sits at one edge and the panel swings outward when opened.
struct SafehouseDoorFrame
{
    float hingeX = 0.f, hingeZ = 0.f;
    float centreX = 0.f, centreZ = 0.f;
    bool spansZ = true;
    float openAngle = 0.f;
};

inline SafehouseDoorFrame safehouseDoorFrame(MazePosition centre, int side) noexcept
{
    SafehouseDoorFrame frame;
    const float hx = static_cast<float>(centre.col) + .5f;
    const float hz = static_cast<float>(centre.row) + .5f;
    switch (side)
    {
    case 0: // -X
        frame = { hx - 1.45f, hz - .59f, hx - 1.45f, hz, true, -98.f };
        break;
    case 1: // +X
        frame = { hx + 1.45f, hz - .59f, hx + 1.45f, hz, true, 98.f };
        break;
    case 2: // -Z
        frame = { hx - .59f, hz - 1.45f, hx, hz - 1.45f, false, 98.f };
        break;
    case 3: // +Z
        frame = { hx - .59f, hz + 1.45f, hx, hz + 1.45f, false, -98.f };
        break;
    default:
        break;
    }
    frame.hingeX = hx + (frame.hingeX - hx) * refugeScale;
    frame.hingeZ = hz + (frame.hingeZ - hz) * refugeScale;
    frame.centreX = hx + (frame.centreX - hx) * refugeScale;
    frame.centreZ = hz + (frame.centreZ - hz) * refugeScale;
    return frame;
}

// Independent of the maze grid so hosts and guests can derive the same refuges
// from its dimensions and seed. Each clearing stays inside the outer boundary.
inline std::vector<MazePosition> generateSafehouses(const MazeSize& size, std::uint32_t seed)
{
    if (!size.isValid()) return {};
    std::vector<MazePosition> candidates;
    for (int row = 3; row <= size.rows - 4; row += 2)
        for (int col = 3; col <= size.cols - 4; col += 2)
            if (row + col >= 6 && (size.rows - 1 - row) + (size.cols - 1 - col) >= 6)
                candidates.push_back({ row, col });
    std::mt19937 random(seed ^ 0x85ebca6bu);
    std::shuffle(candidates.begin(), candidates.end(), random);
    const int smallerSide = std::min(size.rows, size.cols);
    const std::size_t wanted = smallerSide >= 25 ? 4u : 2u;
    const int separation = std::max(6, smallerSide / 4);
    std::vector<MazePosition> best;
    for (std::size_t start = 0; start < std::min<std::size_t>(16, candidates.size()); ++start)
    {
        std::vector<MazePosition> chosen{ candidates[start] };
        for (std::size_t step = 1; step < candidates.size() && chosen.size() < wanted; ++step)
        {
            const auto candidate = candidates[(start + step) % candidates.size()];
            bool separate = true;
            for (const auto other : chosen)
                if (std::max(std::abs(candidate.row - other.row), std::abs(candidate.col - other.col)) < separation)
                { separate = false; break; }
            if (separate) chosen.push_back(candidate);
        }
        if (chosen.size() > best.size()) best = std::move(chosen);
        if (best.size() == wanted) break;
    }
    return best;
}
