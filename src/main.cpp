#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <cstddef>
#include <array>
#include <vector>
#include <functional>
#include <deque>
#include <random>
#include <algorithm>

using namespace std;

namespace Settings {
    constexpr std::size_t GRID_SIZE { 10 };
    constexpr float CUBE_SIZE { .5f };
    constexpr float CUBE_SPACING { .15f };
    constexpr float TICK_SECONDS { .1f };
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

Vector3 surface_normal(GridPos pos){
    Vector3 vec3 {};
    if( pos[0] == 0 )
        vec3.x = -1;
    else if ( pos[0] == static_cast<int>(Settings::GRID_SIZE) - 1)
        vec3.x = 1;

    if( pos[1] == 0)
        vec3.y = -1;
    else if ( pos[1] == static_cast<int>(Settings::GRID_SIZE) - 1)
        vec3.y = 1;

    if( pos[2] == 0)
        vec3.z = -1;
    else if (pos[2] == static_cast<int>(Settings::GRID_SIZE) - 1)
        vec3.z = 1;

    return Vector3Normalize(vec3);
}

struct Heading {
    Axis axis {};
    int sign {};

    Vector3 to_vector3(){
        Vector3 vec3 {};
        switch(axis){
            case Axis::X:
                    vec3.x = sign;
                break;
            case Axis::Y:
                    vec3.y = sign;
                break;
            case Axis::Z:
                    vec3.z = sign;
                break;
        }
        return vec3;
    }

    static Heading from_vec3(const Vector3& vec3){
        if(vec3.x > 0.5f )
                return { Axis::X, 1};
        if(vec3.x < -0.5f) 
                return { Axis::X, -1 };
        if(vec3.y > 0.5f )
                return { Axis::Y, 1 };
        if(vec3.y < -0.5f) 
                return { Axis::Y, -1 };
        if(vec3.z > 0.5f)
                return { Axis::Z, 1 };
        
        return { Axis::Z, -1 };
    }
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
    int mid { static_cast<int>(Settings::GRID_SIZE) / 2 };
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

  auto& body() { return m_body; };
  auto& head() { return m_body.front(); };
  auto& tail() { return m_body.back(); };

  void turn(int direction_sign){
        auto forward { m_direction.to_vector3() };
        auto up { m_normal.to_vector3() };
        auto right { Vector3CrossProduct(forward, up ) };
       
        Vector3 new_dir = direction_sign < 0 ? Vector3Scale(right, -1.0f) : right;
        m_direction = Heading::from_vec3(new_dir);
  }

  StepResult make_step(){
    auto position { m_body.front() };
    auto axis_index { static_cast<size_t>(m_direction.axis) };
    auto moved { position[axis_index] + m_direction.sign };

    if( moved >= 0 && moved <= static_cast<int>(Settings::GRID_SIZE) - 1){
      position[axis_index] = moved;
      return {position, m_direction, m_normal};
    }

    bool hit_max {moved >= static_cast<int>(Settings::GRID_SIZE) };
    auto normal_axis { static_cast<size_t>(m_normal.axis) };

    Heading new_direction { m_normal.axis, (m_normal.sign < 0) ? 1 : -1};
    Heading new_normal { m_direction };

    position[axis_index] = hit_max ? static_cast<int>(Settings::GRID_SIZE) - 1 : 0;
    position[normal_axis] = m_normal.sign < 0 ? 1 : static_cast<int>(Settings::GRID_SIZE) - 2;
    return { position, new_direction, new_normal };
  }
  
  void commit(const StepResult& result, bool grow){
        m_body.push_front(result.position);
        if(!grow)
            m_body.pop_back();
        m_direction = result.direction;
        m_normal = result.normal;
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

    Game (){ reset(); }
    void update(float dt) {
        m_counter += dt;

        if(m_lost){
            if(IsKeyPressed(KEY_R))
                reset();
        }

        if(IsKeyPressed(KEY_A))
            m_snake.turn(-1);
        else if(IsKeyPressed(KEY_D))
            m_snake.turn(1);

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
            DrawCube(base, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, Fade(BLUE, 0.4f));
            return;
        }

        Color color {};

        switch(type){
            case CubeType::SnakeBody:
                color = DARKGREEN;
                break;
            case CubeType::SnakeHead:
                color = GREEN;
                break;
            case CubeType::Food:
                color = RED;
                break;
            default:
                break;
        }
        
        Vector3 offsetPosition = Vector3Add(base, surface_normal(position));
        DrawCube(offsetPosition, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, color);
    }

    void make_tick() {
        auto result { m_snake.make_step() };
        auto type { m_grid.at(result.position) };
        if(type == CubeType::SnakeBody || type == CubeType::SnakeHead){
            m_lost = true;
            return;
        }

        auto ate_food { type ==  CubeType::Food };
        auto old_head { m_snake.head() };
        auto old_tail { m_snake.tail() }; 
        
        m_snake.commit(result, ate_food);
        m_grid.at(old_head) = CubeType::SnakeBody;
        m_grid.at(result.position) = CubeType::SnakeHead;

        if(ate_food){
            spawn_food();
        }else{
            m_grid.at(old_tail) = CubeType::Wall; 
        }
    }

    void reset(){
        m_snake = {};
        m_grid = {};
        m_counter = 0;
        m_lost = false;

        bool is_head { true };
        for(auto& segment : m_snake.body()){
                m_grid.at(segment) = is_head ? CubeType::SnakeHead : CubeType::SnakeBody; 
                is_head = false;
        }

        spawn_food();
    }

    void spawn_food(){

        static mt19937 generator { random_device {}() }; 
        vector<GridPos> candidates {};
        m_grid.foreach_cell([&](CubeType t, GridPos p){
            if(t == CubeType::Wall)
                candidates.push_back(p);
        });

        if(candidates.empty())
            terminate();
        
        GridPos random_candidate {};
        sample(candidates.begin(), candidates.end(), &random_candidate, 1, generator);
        m_grid.at(random_candidate) = CubeType::Food;
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