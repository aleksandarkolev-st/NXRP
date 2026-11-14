#include "Maze.Generation.h"
#include "Maze.Validation.h"
#include "Maze.Safehouse.h"
#include "Maze.Story.h"

#include <algorithm>
#include <random>
#include <charconv>
#include <stdexcept>
#include <vector>

MazeGrid generateMaze(const MazeSize& size, std::uint32_t seed)
{
    MazeGrid maze(size);
    std::mt19937 random(seed);
    std::vector<MazePosition> path;
    path.reserve(static_cast<std::size_t>((size.rows + 1) / 2)
        * static_cast<std::size_t>((size.cols + 1) / 2));
    path.push_back({ 0, 0 });
    maze.at(0, 0) = MazeCell::Passage;

    // Even row/column positions are rooms; a one-cell wall separates them.
    constexpr MazePosition directions[] = { { -2, 0 }, { 2, 0 }, { 0, -2 }, { 0, 2 } };
    while (!path.empty())
    {
        const auto current = path.back();
        MazePosition candidates[4];
        int count = 0;
        for (const auto direction : directions)
        {
            const MazePosition next{ current.row + direction.row, current.col + direction.col };
            if (maze.contains(next) && maze.at(next.row, next.col) == MazeCell::Wall)
                candidates[count++] = next;
        }

        if (count == 0)
        {
            path.pop_back();
            continue;
        }
        std::uniform_int_distribution<int> choose(0, count - 1);
        const auto next = candidates[choose(random)];
        maze.at((current.row + next.row) / 2, (current.col + next.col) / 2) = MazeCell::Passage;
        maze.at(next.row, next.col) = MazeCell::Passage;
        path.push_back(next);
    }

    // Open a seeded selection of interior walls to create alternate routes.
    // The original spanning tree remains intact, so every passage stays reachable.
    std::vector<MazePosition> shortcuts;
    for (int row = 1; row < size.rows - 1; ++row)
        for (int col = 1; col < size.cols - 1; ++col)
        {
            if (maze.at(row, col) != MazeCell::Wall) continue;
            const bool vertical = maze.at(row-1, col) == MazeCell::Passage && maze.at(row+1, col) == MazeCell::Passage;
            const bool horizontal = maze.at(row, col-1) == MazeCell::Passage && maze.at(row, col+1) == MazeCell::Passage;
            if (vertical || horizontal) shortcuts.push_back({ row, col });
        }
    std::shuffle(shortcuts.begin(), shortcuts.end(), random);
    const auto rooms = static_cast<std::size_t>((size.rows + 1) / 2) * static_cast<std::size_t>((size.cols + 1) / 2);
    const auto shortcutCount = std::min(shortcuts.size(), std::max<std::size_t>(2, rooms / 8));
    for (std::size_t i = 0; i < shortcutCount; ++i)
        maze.at(shortcuts[i].row, shortcuts[i].col) = MazeCell::Passage;

    // Carve refuges without removing any part of the original spanning tree.
    for (const auto center : generateSafehouses(size, seed))
    {
        // A walkable outer ring lets the hunter route around the protected
        // 5x5 interior even when the original maze passed through this spot.
        for (int row = center.row - 3; row <= center.row + 3; ++row)
            for (int col = center.col - 3; col <= center.col + 3; ++col)
                maze.at(row, col) = MazeCell::Passage;
    }

    // A collapsed sourced cabin occupies a narrow core. Its carved bypass
    // reconnects all original approaches and never removes a refuge interior.
    if(const auto ruin=generateStoryRuin(size,seed))
    {
        for(int row=0;row<size.rows;++row)for(int col=0;col<size.cols;++col)
            if(storyRuinReserved({row,col},ruin))
                maze.at(row,col)=storyRuinBlocked({row,col},ruin)?MazeCell::Wall:MazeCell::Passage;
    }

    // For even dimensions the last row/column lies beyond the room lattice.
    // Extend its already-connected bottom-right room by at most two cells.
    const int lastRow = size.rows - 1;
    const int lastCol = size.cols - 1;
    const int roomRow = lastRow - lastRow % 2;
    const int roomCol = lastCol - lastCol % 2;
    for (int row = roomRow; row <= lastRow; ++row)
        maze.at(row, roomCol) = MazeCell::Passage;
    for (int col = roomCol; col <= lastCol; ++col)
        maze.at(lastRow, col) = MazeCell::Passage;
    maze.at(lastRow, lastCol) = MazeCell::Exit;
    return maze;
}

MazeGrid generateMaze(const MazeSize& size)
{
    if (!isValidMazeSize(size))
        throw std::invalid_argument("Maze dimensions must each be between 10 and 100.");
    return generateMaze(size, generateMazeSeed());
}

std::uint32_t generateMazeSeed()
{
    std::random_device entropy;
    return static_cast<std::uint32_t>(entropy());
}

bool parseMazeSeed(std::string_view text, std::optional<std::uint32_t>& seed) noexcept
{
    if (text.empty())
    {
        seed.reset();
        return true;
    }
    if (text.size() > 10) return false;

    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return false;
    seed = value;
    return true;
}
