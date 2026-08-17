#include <iostream>
#include <raylib.h>
#include <cstddef>
#include <array>
#include <functional>

using namespace std;

namespace Settings {
    constexpr std::size_t GRID_SIZE { 10 };
    constexpr float CUBE_SIZE { .5f };
    constexpr float CUBE_SPACING { .15f };
    constexpr float TICK_SECONDS { .3f };
}

using GridPos = array<int, 3>;

Vector3 get_world_position(const GridPos& pos) {
    using namespace Settings;
    return Vector3 {
        pos[0] * (CUBE_SIZE + CUBE_SPACING),
        pos[1] * (CUBE_SIZE + CUBE_SPACING),
        pos[2] * (CUBE_SIZE + CUBE_SPACING)
    };
}

enum class Axis {
    X = 0,
    Y = 1,
    Z = 2
};

struct Heading {
    Axis axis {};
    int sign {};
};

Camera3D create_camera() {
    using namespace Settings;
    Camera3D camera {};

    float target { (CUBE_SIZE + CUBE_SPACING) * (GRID_SIZE - 1) / 2.0f };

    camera.position = { 10, 10, 10 };
    camera.target = { target, target, target };
    camera.fovy = 60;
    camera.up = { 0, 1, 0 };
    camera.projection = CAMERA_PERSPECTIVE;

    return camera;
}

enum class CubeType {
    None,
    Wall,
    SnakeBody,
    SnakeHead,
    Food,
};

class Snake{

public:
  Snake(){
    int mid { Settings::GRID_SIZE / 2 };
    for(int i {}; i < 3; i++){
        GridPos p { mid - i, 0, mid };
        m_body.push_back(p);
    }
  }
  struct StepResult{
    GridPos position {};
    Heading direction {};
    Heading normal {};
  };

  StepResult make_step(){
    auto position {m_body.front() };
    auto axis_index { static_cast<size_t>(m_direction.axis) };
    auto moved { position[axis_index] + m_direction.sign };

    if( moved >= 0 && moved <= Settings::GRID_SIZE - 1){
      position[axis_index] = moved;
      return {position, m_direction, m_normal};
    }

    bool hit_max {moved >= Settings::GRID_SIZE };

    Heading new_direction { m_normal.axis, (m_normal.sign < 0) ? 1 : -1};
    Heading
  }

private:
    deque<GridPos> m_body{};
    Heading m_direction {Axis::X, 1};
    Heading m_normal { Axis::Y, -1};

};

class Grid {
public:
    Grid() {
        for (int x {}; x < static_cast<int>(Settings::GRID_SIZE); x++) {
            for (int y {}; y < static_cast<int>(Settings::GRID_SIZE); y++) {
                for (int z {}; z < static_cast<int>(Settings::GRID_SIZE); z++) {
                    GridPos pos { x, y, z };
                    at(pos) = is_boundary(pos) ? CubeType::Wall : CubeType::None;
                }
            }
        }
    }

    CubeType& at(GridPos pos) {
        return m_grid[pos[0]][pos[1]][pos[2]];
    }

    void foreach_cell(const function<void(CubeType, GridPos)>& callable) {
        for (int x {}; x < static_cast<int>(Settings::GRID_SIZE); x++) {
            for (int y {}; y < static_cast<int>(Settings::GRID_SIZE); y++) {
                for (int z {}; z < static_cast<int>(Settings::GRID_SIZE); z++) {
                    GridPos pos { x, y, z };
                    callable(at(pos), pos);
                }
            }
        }
    }

private:
    static bool is_boundary(GridPos pos) {
        auto is_edge = [](int k) {
            return k == 0 || k == static_cast<int>(Settings::GRID_SIZE) - 1;
        };
        return is_edge(pos[0]) || is_edge(pos[1]) || is_edge(pos[2]);
    }

private:
    using Layer = array<array<CubeType, Settings::GRID_SIZE>, Settings::GRID_SIZE>;
    array<Layer, Settings::GRID_SIZE> m_grid {};
};

class Game {
public:
    void update(float dt) {
        m_counter += dt;

        if (m_counter > Settings::TICK_SECONDS) {
            make_tick();
            m_counter = 0;
        }
    }

    void render() {
        m_grid.foreach_cell([&](CubeType t, GridPos p) {
            render_cell(t, p);
        });
    }

private:
    void render_cell(CubeType type, GridPos position) {
        using namespace Settings;

        if (type == CubeType::None)
            return;

        Vector3 base { get_world_position(position) };

        if (type == CubeType::Wall) {
            DrawCube(base, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, Fade(BLUE,0.4f));
            return;
        }
    }

    void make_tick() {
     
    }

private:
    Snake m_snake {};
    Grid m_grid {};
    float m_counter {};
    bool m_lost {};
};

int main() {
    InitWindow(1000, 1000, "3d_snake");
    SetTargetFPS(60);

    auto camera { create_camera() };
    Game game {};

    while (!WindowShouldClose()) {
        game.update(GetFrameTime());

        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
        game.render();
        EndMode3D();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}