#include "pch.h"
#include "CppUnitTest.h"
#include "Maze.Game.h"
#include "Maze.Generation.h"
#include "Maze.Safehouse.h"
#include "Maze.Story.h"
#include "Maze.Puzzle.h"
#include "Maze.Adventure.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace MazeUnitTest
{
    namespace
    {
        constexpr std::uint32_t fixedSeeds[] = { 0, 1, 42, 12345, 0x80000000U, UINT32_MAX };
        const MazeSize invalidSizes[] = {
            { 9, 10 }, { 101, 10 }, { 10, 9 }, { 10, 101 }, { 0, 10 }, { 10, 0 },
            { -1, -1 }, { INT_MIN, 10 }, { INT_MAX, 10 }, { 10, INT_MIN }, { 10, INT_MAX }
        };

        std::wstring Context(const MazeSize& size, std::uint32_t seed)
        {
            return L"rows=" + std::to_wstring(size.rows) + L", cols=" + std::to_wstring(size.cols)
                + L", seed=" + std::to_wstring(seed);
        }

        std::vector<MazeCell> ReadCells(const MazeGrid& maze)
        {
            std::vector<MazeCell> result;
            for (int row = 0; row < maze.size().rows; ++row)
                for (int col = 0; col < maze.size().cols; ++col)
                    result.push_back(maze.at(row, col));
            return result;
        }

        struct RouteStep
        {
            MoveDirection direction;
            MazePosition destination;
        };

        std::vector<RouteStep> AssertConnectedMaze(const MazeGrid& maze, const MazeSize& size,
            const wchar_t* context)
        {
            Assert::IsFalse(maze.empty(), context);
            Assert::AreEqual(size.rows, maze.size().rows, context);
            Assert::AreEqual(size.cols, maze.size().cols, context);
            const int total = size.rows * size.cols;
            Assert::AreEqual(static_cast<std::size_t>(total), maze.cellCount(), context);
            int exits = 0, walls = 0, walkable = 0;
            for (const auto cell : ReadCells(maze))
            {
                Assert::IsTrue(cell == MazeCell::Wall || cell == MazeCell::Passage || cell == MazeCell::Exit, context);
                exits += cell == MazeCell::Exit;
                walls += cell == MazeCell::Wall;
                walkable += cell != MazeCell::Wall;
            }
            Assert::AreEqual(1, exits, context);
            Assert::IsTrue(walls > 0, context);
            Assert::IsTrue(maze.at(0, 0) == MazeCell::Passage, context);
            Assert::IsTrue(maze.at(size.rows - 1, size.cols - 1) == MazeCell::Exit, context);

            // This independent BFS reads one-cell neighbors. It does not call
            // tryMove or reuse the generator's two-cell DFS carving rules.
            const struct { int row, col; MoveDirection direction; } steps[] = {
                { -1, 0, MoveDirection::Up }, { 1, 0, MoveDirection::Down },
                { 0, -1, MoveDirection::Left }, { 0, 1, MoveDirection::Right }
            };
            std::vector<int> parent(static_cast<std::size_t>(total), -1);
            std::vector<MoveDirection> arrival(static_cast<std::size_t>(total));
            std::queue<int> pending;
            parent[0] = 0;
            pending.push(0);
            int reached = 0;
            while (!pending.empty())
            {
                const int current = pending.front();
                pending.pop();
                ++reached;
                for (const auto step : steps)
                {
                    const int row = current / size.cols + step.row;
                    const int col = current % size.cols + step.col;
                    if (row < 0 || row >= size.rows || col < 0 || col >= size.cols) continue;
                    const int next = row * size.cols + col;
                    if (parent[next] != -1 || maze.at(row, col) == MazeCell::Wall) continue;
                    parent[next] = current;
                    arrival[next] = step.direction;
                    pending.push(next);
                }
            }
            Assert::IsTrue(parent[total - 1] != -1, context);
            Assert::AreEqual(walkable, reached, context);
            std::vector<RouteStep> route;
            for (int cell = total - 1; cell != 0; cell = parent[cell])
                route.push_back({ arrival[cell], { cell / size.cols, cell % size.cols } });
            std::reverse(route.begin(), route.end());
            return route;
        }

        void AssertSizeForFixedSeeds(const MazeSize& size)
        {
            for (const auto seed : fixedSeeds)
            {
                const auto context = Context(size, seed);
                const auto maze = generateMaze(size, seed);
                (void)AssertConnectedMaze(maze, size, context.c_str());
            }
        }
    }

    TEST_CLASS(MazeGenerationTests)
    {
    public:
        TEST_METHOD(AdventureTransactionsAreAtomicAndBounded)
        {
            Adventure::Pack pack;const auto initial=pack;
            Assert::IsFalse(Adventure::buy(pack,4));Assert::AreEqual(initial.coins,pack.coins);
            Assert::IsTrue(Adventure::buy(pack,0));Assert::AreEqual(6u,pack.coins);Assert::AreEqual(2u,pack.items[0]);
            Assert::IsTrue(Adventure::craft(pack,0));Assert::AreEqual(0u,pack.materials[0]);
            const auto before=pack;Assert::IsFalse(Adventure::craft(pack,0));Assert::IsTrue(before.materials==pack.materials && before.items==pack.items);
            Assert::IsFalse(Adventure::sell(pack));Assert::AreEqual(before.coins,pack.coins);
            pack.materials={1,1,1};Assert::IsTrue(Adventure::sell(pack));Assert::AreEqual(15u,pack.coins);
            pack.items[0]=99;Assert::IsFalse(Adventure::buy(pack,0));Assert::AreEqual(15u,pack.coins);
            pack.effects={12,15,20};pack.echoTime=8;Adventure::tick(pack,30);Assert::IsTrue(pack.effects==std::array<float,3>{});Assert::AreEqual(0.f,pack.echoTime);
            Assert::IsTrue(Adventure::valid(pack));pack.effects[0]=13;Assert::IsFalse(Adventure::valid(pack));
        }
        TEST_METHOD(AdventureCachesAreSeededPassableAndSpaced)
        {
            for(const MazeSize size:{MazeSize{10,10},MazeSize{30,30},MazeSize{100,100}})
            for(unsigned seed=0;seed<12;++seed)
            {
                const auto maze=generateMaze(size,seed);const auto puzzle=generateMazePuzzle(maze,seed);
                const auto caches=Adventure::caches(maze,seed,puzzle.stones),replay=Adventure::caches(maze,seed,puzzle.stones);
                Assert::IsTrue(!caches.empty() && caches.size()<=48 && caches.size()==replay.size());
                for(std::size_t i=0;i<caches.size();++i)
                {
                    const auto cache=caches[i];Assert::IsTrue(maze.at(cache.cell.row,cache.cell.col)==MazeCell::Passage);
                    Assert::IsTrue(cache.kind>=0 && cache.kind<3 && cache.coins>=3 && cache.coins<=7);
                    Assert::AreEqual(cache.cell.row,replay[i].cell.row);Assert::AreEqual(cache.cell.col,replay[i].cell.col);
                    Assert::IsTrue(safehouseAt(cache.cell,generateSafehouses(size,seed))<0);
                    for(std::size_t j=0;j<i;++j)Assert::IsTrue(std::abs(cache.cell.row-caches[j].cell.row)+std::abs(cache.cell.col-caches[j].cell.col)>=3);
                }
            }
        }
        TEST_METHOD(CollapsedSheltersHaveOpenBypassesAndPreserveRefuges)
        {
            for(const MazeSize size: {MazeSize{10,10},MazeSize{10,100},MazeSize{25,30},MazeSize{100,100}})
            for(unsigned seed=0;seed<16;++seed)
            {
                const auto ruin=generateStoryRuin(size,seed);Assert::IsTrue(ruin.has_value());
                const auto maze=generateMaze(size,seed);const auto houses=generateSafehouses(size,seed);
                int blocked=0;
                for(int r=0;r<size.rows;++r)for(int c=0;c<size.cols;++c)
                    if(storyRuinReserved({r,c},ruin))
                    {
                        Assert::IsTrue(safehouseAt({r,c},houses)<0);
                        if(storyRuinBlocked({r,c},ruin))
                        {++blocked;Assert::IsTrue(maze.at(r,c)==MazeCell::Wall);}
                        else Assert::IsTrue(maze.at(r,c)==MazeCell::Passage);
                    }
                Assert::AreEqual(3,blocked);
                const auto replay=generateStoryRuin(size,seed);
                Assert::AreEqual(ruin->cell.row,replay->cell.row);Assert::AreEqual(ruin->cell.col,replay->cell.col);
            }
        }
        TEST_METHOD(StorySitesAndBootTrailsAreReachableAndReproducible)
        {
            for(const MazeSize size: {MazeSize{10,10},MazeSize{20,30},MazeSize{100,100}})
            for(unsigned seed=0;seed<8;++seed)
            {
                const auto maze=generateMaze(size,seed);const auto puzzle=generateMazePuzzle(maze,seed);
                const auto sites=generateStorySites(maze,seed,puzzle.stones),replay=generateStorySites(maze,seed,puzzle.stones);
                for(int i=0;i<4;++i)
                {
                    Assert::IsTrue(maze.at(sites[i].cell.row,sites[i].cell.col)==MazeCell::Passage);
                    Assert::AreEqual(sites[i].cell.row,replay[i].cell.row);Assert::AreEqual(sites[i].cell.col,replay[i].cell.col);
                    for(const auto stone:puzzle.stones)Assert::IsFalse(stone.row==sites[i].cell.row && stone.col==sites[i].cell.col);
                }
                const auto houses=generateSafehouses(size,seed);
                const auto prints=generateStoryFootsteps(maze,sites,houses);
                Assert::IsTrue(!prints.empty() && prints.size()<=24);
                for(auto print:prints)
                {
                    const MazePosition at{static_cast<int>(print.z),static_cast<int>(print.x)};
                    Assert::IsTrue(maze.contains(at) && maze.at(at.row,at.col)==MazeCell::Passage);
                    Assert::IsTrue(safehouseAt(at,houses)<0);
                    Assert::IsTrue(std::abs(print.cosine*print.cosine+print.sine*print.sine-1.f)<.0001f);
                }
            }
        }
        TEST_METHOD(SeededSafehousesAreDistinctOpenAndReproducible)
        {
            const MazeSize sizes[] = { {10,10}, {10,100}, {30,30}, {99,100}, {100,100} };
            for (const auto size : sizes) for (std::uint32_t seed = 0; seed < 32; ++seed)
            {
                const auto maze = generateMaze(size, seed);
                const auto houses = generateSafehouses(size, seed);
                const auto replay = generateSafehouses(size, seed);
                Assert::IsTrue(houses.size() >= (size.rows < 15 && size.cols < 15 ? 1u : 2u) && houses.size() <= 4);
                Assert::AreEqual(houses.size(), replay.size());
                for (std::size_t i = 0; i < houses.size(); ++i)
                {
                    Assert::IsTrue(houses[i].row == replay[i].row && houses[i].col == replay[i].col);
                    Assert::IsFalse(insideSafehouse({0,0}, houses[i]));
                    Assert::IsFalse(insideSafehouse({size.rows-1,size.cols-1}, houses[i]));
                    for (int row = houses[i].row-2; row <= houses[i].row+2; ++row)
                        for (int col = houses[i].col-2; col <= houses[i].col+2; ++col)
                        {
                            Assert::IsTrue(maze.at(row,col) == MazeCell::Passage);
                            Assert::AreEqual(static_cast<int>(i), safehouseAt({row,col}, houses));
                        }
                    Assert::IsTrue(maze.at(houses[i].row - 2, houses[i].col) == MazeCell::Passage);
                    Assert::IsTrue(maze.at(houses[i].row + 2, houses[i].col) == MazeCell::Passage);
                    Assert::IsTrue(maze.at(houses[i].row, houses[i].col - 2) == MazeCell::Passage);
                    Assert::IsTrue(maze.at(houses[i].row, houses[i].col + 2) == MazeCell::Passage);
                }
            }
        }

        TEST_METHOD(SafehouseDoorFramesFillEachOpeningAndSwingOutward)
        {
            const MazePosition house{ 10, 12 };
            const float hx = 12.5f, hz = 10.5f;
            const auto nearly = [](float a, float b) { return std::abs(a - b) < .0001f; };
            const auto places = [&](int side, float hingeX, float hingeZ, float centreX,
                float centreZ, bool spansZ, bool anglePositive)
            {
                const auto frame = safehouseDoorFrame(house, side);
                Assert::IsTrue(nearly(frame.hingeX, hx + (hingeX - hx) * refugeScale) && nearly(frame.hingeZ, hz + (hingeZ - hz) * refugeScale));
                Assert::IsTrue(nearly(frame.centreX, hx + (centreX - hx) * refugeScale) && nearly(frame.centreZ, hz + (centreZ - hz) * refugeScale));
                Assert::AreEqual(spansZ, frame.spansZ);
                Assert::IsTrue(anglePositive == (frame.openAngle > 0.f));
            };
            places(0, hx - 1.45f, hz - .59f, hx - 1.45f, hz, true, false);
            places(1, hx + 1.45f, hz - .59f, hx + 1.45f, hz, true, true);
            places(2, hx - .59f, hz - 1.45f, hx, hz - 1.45f, false, true);
            places(3, hx - .59f, hz + 1.45f, hx, hz + 1.45f, false, false);
        }

        TEST_METHOD(RefugesDoNotDisconnectHunterRoutes)
        {
            const MazeSize sizes[] = { {10,10}, {10,100}, {30,30}, {65,90}, {100,100} };
            for (const auto size : sizes) for (std::uint32_t seed = 0; seed < 32; ++seed)
            {
                const auto maze = generateMaze(size, seed);
                const auto houses = generateSafehouses(size, seed);
                std::vector<bool> reached(maze.cellCount(), false);
                std::queue<MazePosition> pending; pending.push({0,0}); reached[0] = true;
                constexpr MazePosition directions[] = { {-1,0}, {1,0}, {0,-1}, {0,1} };
                while (!pending.empty())
                {
                    const auto current = pending.front(); pending.pop();
                    for (const auto d : directions)
                    {
                        const MazePosition next{current.row+d.row,current.col+d.col};
                        if (!maze.contains(next) || maze.at(next.row,next.col)==MazeCell::Wall || safehouseAt(next,houses)>=0) continue;
                        const auto index = static_cast<std::size_t>(next.row*size.cols+next.col);
                        if (!reached[index]) { reached[index]=true; pending.push(next); }
                    }
                }
                for (int row=0; row<size.rows; ++row) for (int col=0; col<size.cols; ++col)
                    if (maze.at(row,col)!=MazeCell::Wall && safehouseAt({row,col},houses)<0)
                        Assert::IsTrue(reached[static_cast<std::size_t>(row*size.cols+col)]);
            }
        }
        TEST_METHOD(SeededMazesIncludeAlternateRoutes)
        {
            const MazeSize sizes[] = { {10, 10}, {11, 12}, {30, 25}, {50, 50} };
            for (const auto size : sizes) for (std::uint32_t seed = 0; seed < 16; ++seed)
            {
                const auto maze = generateMaze(size, seed);
                int vertices = 0, edges = 0;
                for (int row = 0; row < size.rows; ++row) for (int col = 0; col < size.cols; ++col)
                {
                    if (maze.at(row, col) == MazeCell::Wall) continue;
                    ++vertices;
                    if (row + 1 < size.rows && maze.at(row + 1, col) != MazeCell::Wall) ++edges;
                    if (col + 1 < size.cols && maze.at(row, col + 1) != MazeCell::Wall) ++edges;
                }
                // For a connected graph, at least as many edges as vertices means a cycle.
                Assert::IsTrue(edges >= vertices);
            }
        }

        TEST_METHOD(GeneratesMinimumSizeWithReachableExit)
        {
            AssertSizeForFixedSeeds({ 10, 10 });
        }

        TEST_METHOD(GeneratesIntermediateSizeWithReachableExit)
        {
            AssertSizeForFixedSeeds({ 25, 37 });
        }

        TEST_METHOD(GeneratesMaximumSizeWithReachableExit)
        {
            AssertSizeForFixedSeeds({ MAX_MAZE_SIZE, MAX_MAZE_SIZE });
        }

        TEST_METHOD(SupportsEvenAndOddRectangles)
        {
            const MazeSize sizes[] = { { 10, 50 }, { 50, 10 }, { 11, 12 },
                { 12, 11 }, { 49, 50 }, { 50, 49 } };
            for (const auto& size : sizes) AssertSizeForFixedSeeds(size);
        }

        TEST_METHOD(ReproducesMapsForFixedSeeds)
        {
            for (const MazeSize size : { MazeSize(10, 10), MazeSize(25, 37), MazeSize(50, 50) })
            {
                for (const auto seed : fixedSeeds)
                {
                    const auto context = Context(size, seed);
                    const auto first = generateMaze(size, seed);
                    const auto repeated = generateMaze(size, seed);
                    Assert::AreEqual(first.size().rows, repeated.size().rows, context.c_str());
                    Assert::AreEqual(first.size().cols, repeated.size().cols, context.c_str());
                    Assert::IsTrue(ReadCells(first) == ReadCells(repeated), context.c_str());
                }
            }
        }

        TEST_METHOD(RejectsInvalidSizesWithSeed)
        {
            for (const auto& size : invalidSizes)
            {
                const auto context = Context(size, 42);
                Assert::ExpectException<std::invalid_argument>([size] {
                    (void)generateMaze(size, 42);
                }, context.c_str());
            }
        }

        TEST_METHOD(RejectsInvalidSizesWithoutSeed)
        {
            for (const auto& size : invalidSizes)
            {
                const auto context = Context(size, 0);
                Assert::ExpectException<std::invalid_argument>([size] {
                    (void)generateMaze(size);
                }, context.c_str());
            }
        }

        TEST_METHOD(AllAllowedDimensionsHaveReachableExit)
        {
            for (int rows = MIN_MAZE_SIZE; rows <= MAX_MAZE_SIZE; ++rows)
            {
                for (int cols = MIN_MAZE_SIZE; cols <= MAX_MAZE_SIZE; ++cols)
                {
                    const MazeSize size(rows, cols);
                    const auto seed = static_cast<std::uint32_t>(rows * 100 + cols);
                    const auto context = Context(size, seed);
                    const auto maze = generateMaze(size, seed);
                    (void)AssertConnectedMaze(maze, size, context.c_str());
                }
            }
        }

        TEST_METHOD(GeneratedMazesCanBePlayedToVictory)
        {
            const MazeSize sizes[] = { { 10, 10 }, { 25, 37 }, { 50, 50 },
                { 10, 50 }, { 50, 10 }, { 11, 12 } };
            for (const auto& size : sizes)
            {
                for (const auto seed : fixedSeeds)
                {
                    const auto context = Context(size, seed);
                    auto maze = generateMaze(size, seed);
                    const auto original = ReadCells(maze);
                    const auto route = AssertConnectedMaze(maze, size, context.c_str());
                    GameState game;
                    startGame(game, std::move(maze));
                    Assert::AreEqual(0, game.player.row, context.c_str());
                    Assert::AreEqual(0, game.player.col, context.c_str());
                    Assert::IsFalse(hasWon(game), context.c_str());
                    for (std::size_t index = 0; index < route.size(); ++index)
                    {
                        const auto expected = index + 1 == route.size() ? MoveResult::Won : MoveResult::Moved;
                        Assert::IsTrue(tryMove(game, route[index].direction) == expected, context.c_str());
                        Assert::AreEqual(route[index].destination.row, game.player.row, context.c_str());
                        Assert::AreEqual(route[index].destination.col, game.player.col, context.c_str());
                    }
                    Assert::IsTrue(hasWon(game), context.c_str());
                    Assert::IsTrue(game.phase == GamePhase::Won, context.c_str());
                    Assert::AreEqual(size.rows - 1, game.player.row, context.c_str());
                    Assert::AreEqual(size.cols - 1, game.player.col, context.c_str());
                    Assert::IsTrue(original == ReadCells(game.maze), context.c_str());
                }
            }
        }

        TEST_METHOD(UnseededMazesAreValid)
        {
            for (const MazeSize size : { MazeSize(10, 10), MazeSize(25, 37), MazeSize(50, 50) })
            {
                const auto context = Context(size, 0);
                const auto maze = generateMaze(size);
                (void)AssertConnectedMaze(maze, size, context.c_str());
            }
        }

        TEST_METHOD(KnownFixedSeedsShowVariation)
        {
            std::set<std::vector<MazeCell>> layouts;
            for (const auto seed : fixedSeeds)
                layouts.insert(ReadCells(generateMaze(MazeSize(10, 10), seed)));
            // This fixed sample must demonstrate variation; individual maps may repeat.
            Assert::IsTrue(layouts.size() > 1, L"All sampled fixed seeds produced the same layout.");
        }
    };
}
