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
#include <cmath>
using namespace std;

namespace Settings {
    constexpr std::size_t GRID_SIZE { 10 };
    constexpr float CUBE_SIZE { .5f };
    constexpr float CUBE_SPACING { .15f };

    // Ritmo do jogo: começa devagar e acelera aos poucos conforme a cobra cresce,
    // igual ao snake clássico (nunca fica impossível, tem um piso de velocidade).
    constexpr float TICK_SECONDS_BASE  { 0.20f };
    constexpr float TICK_SECONDS_MIN   { 0.08f };
    constexpr float TICK_SECONDS_DECAY { 0.006f }; // reduzido por segmento comido

    constexpr float CAMERA_DISTANCE { 14.0f };
    constexpr float CAMERA_LOOK_SPEED { 10.0f }; // suavização (independente de framerate)

    constexpr int SNAKE_INITIAL_LENGTH { 3 };
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

// Interpolação esférica entre dois vetores unitários — mantém velocidade angular
// constante (diferente de lerp+normalize, que acelera/desacelera de forma irregular).
Vector3 vector3_slerp(Vector3 a, Vector3 b, float t) {
    float dot = Clamp(Vector3DotProduct(a, b), -1.0f, 1.0f);
    if (dot > 0.9995f) {
        // Quase paralelos: lerp comum já é suficiente e evita divisão por ~0.
        return Vector3Normalize(Vector3Lerp(a, b, t));
    }
    float theta = acosf(dot) * t;
    Vector3 relative = Vector3Normalize(Vector3Subtract(b, Vector3Scale(a, dot)));
    return Vector3Add(Vector3Scale(a, cosf(theta)), Vector3Scale(relative, sinf(theta)));
}

// Suavização exponencial independente de framerate (substitui o "dt * N" cru).
float smooth_step(float current, float target, float speed, float dt) {
    float t = 1.0f - expf(-speed * dt);
    return current + (target - current) * t;
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
    Vector3 to_vector3() const {
        Vector3 vec3 {};
        switch(axis){
            case Axis::X: vec3.x = sign; break;
            case Axis::Y: vec3.y = sign; break;
            case Axis::Z: vec3.z = sign; break;
        }
        return vec3;
    }
    static Heading from_vec3(const Vector3& vec3){
        if(vec3.x > 0.5f ) return { Axis::X, 1};
        if(vec3.x < -0.5f) return { Axis::X, -1 };
        if(vec3.y > 0.5f ) return { Axis::Y, 1 };
        if(vec3.y < -0.5f) return { Axis::Y, -1 };
        if(vec3.z > 0.5f ) return { Axis::Z, 1 };

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
    for(int i {}; i < Settings::SNAKE_INITIAL_LENGTH; i++){
        GridPos p { mid - i, 0, mid };
        m_body.push_back(p);
    }
    sync_previous();
  }

  struct StepResult{
    GridPos position {};
    Heading direction {};
    Heading normal {};
  };
  auto& body() { return m_body; }
  const auto& body() const { return m_body; }
  auto& head() { return m_body.front(); }
  auto& tail() { return m_body.back(); }
  Heading get_normal() const { return m_normal; }

  // Acesso aos dados do tick anterior para fazer a interpolação visual suave
  const deque<GridPos>& previous_body() const { return m_previous_body; }
  Heading get_previous_normal() const { return m_previous_normal; }
  void sync_previous() {
      m_previous_body = m_body;
      m_previous_normal = m_normal;
  }
  void set_direction(Vector3 input_dir) {
        // Compara com a direção JÁ ENFILEIRADA (não com a atual), senão dois
        // inputs rápidos entre ticks podem enfileirar uma curva de 180°.
        Vector3 pending_dir = m_next_direction.to_vector3();
        if (Vector3DotProduct(input_dir, pending_dir) > -0.5f) {
            m_next_direction = Heading::from_vec3(input_dir);
        }
  }
  StepResult make_step(){
    m_direction = m_next_direction;
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
        // Salva o estado atual antes de mover, para permitir a animação suave do frame atual
        sync_previous();
        m_body.push_front(result.position);
        if(!grow)
            m_body.pop_back();

        m_direction = result.direction;
        m_next_direction = result.direction;
        m_normal = result.normal;
  }
private:
    deque<GridPos> m_body{};
    deque<GridPos> m_previous_body{};

    Heading m_direction {Axis::X, 1};
    Heading m_next_direction {Axis::X, 1};

    Heading m_normal { Axis::Y, -1};
    Heading m_previous_normal { Axis::Y, -1};
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

    Vector3 get_face_up(Vector3 normal) {
        if (std::abs(normal.y) > 0.5f) return {0, 0, -normal.y};
        return {0, 1, 0};
    }
    Vector3 get_interpolated_pos(GridPos current, GridPos previous, float progress) {
        Vector3 cur_base = get_world_position(current);
        Vector3 cur_norm = surface_normal(current);
        Vector3 cur_pos = Vector3Add(cur_base, cur_norm);

        Vector3 prev_base = get_world_position(previous);
        Vector3 prev_norm = surface_normal(previous);
        Vector3 prev_pos = Vector3Add(prev_base, prev_norm);

        return Vector3Lerp(prev_pos, cur_pos, progress);
    }

    // Duração do tick atual: começa em TICK_SECONDS_BASE e vai encurtando
    // conforme a cobra cresce, com piso em TICK_SECONDS_MIN.
    float tick_seconds() const {
        using namespace Settings;
        int grown = static_cast<int>(m_snake.body().size()) - SNAKE_INITIAL_LENGTH;
        float t = TICK_SECONDS_BASE - grown * TICK_SECONDS_DECAY;
        return std::max(TICK_SECONDS_MIN, t);
    }

    void update(float dt, Camera3D& camera) {
        m_current_tick = tick_seconds();
        m_counter += dt;

        if (m_lost) {
            if (IsKeyPressed(KEY_R)) reset();
            return; // trava o jogo inteiro (sem input, sem tick, câmera parada)
        }

        Vector3 logic_normal = m_snake.get_normal().to_vector3();
        Vector3 logic_face_up = get_face_up(logic_normal);
        Vector3 logic_face_right = Vector3CrossProduct(logic_face_up, logic_normal);

        Vector3 input_dir {0, 0, 0};
        if (IsKeyPressed(KEY_W)) input_dir = logic_face_up;
        if (IsKeyPressed(KEY_S)) input_dir = Vector3Scale(logic_face_up, -1.0f);
        if (IsKeyPressed(KEY_D)) input_dir = logic_face_right;
        if (IsKeyPressed(KEY_A)) input_dir = Vector3Scale(logic_face_right, -1.0f);
        if (Vector3Length(input_dir) > 0.5f) {
            m_snake.set_direction(input_dir);
        }

        if (m_counter > m_current_tick) {
            make_tick();
            m_counter -= m_current_tick;
        }

        float progress = std::clamp(m_counter / m_current_tick, 0.0f, 1.0f);

        float center_val = (Settings::CUBE_SIZE + Settings::CUBE_SPACING) * (Settings::GRID_SIZE - 1) / 2.0f;
        Vector3 center = { center_val, center_val, center_val };

        GridPos cur_head = m_snake.head();
        GridPos prev_head = m_snake.previous_body().empty() ? cur_head : m_snake.previous_body().front();
        Vector3 visual_head = get_interpolated_pos(cur_head, prev_head, progress);

        Vector3 dir_to_head = Vector3Normalize(Vector3Subtract(visual_head, center));

        Vector3 target_pos = Vector3Add(center, Vector3Scale(dir_to_head, Settings::CAMERA_DISTANCE));

        Vector3 prev_normal = m_snake.get_previous_normal().to_vector3();
        Vector3 prev_face_up = get_face_up(prev_normal);

        // Slerp com velocidade angular constante -> giro de quina redondo, sem "puxão".
        Vector3 target_up = vector3_slerp(prev_face_up, logic_face_up, progress);

        // target_pos/target_up JÁ são a interpolação suave (baseada no progresso do
        // tick) — atribuir direto em vez de aplicar outro lerp por cima é o que
        // deixa a câmera "colada" na cobra, sem atraso/duplo suavizador brigando.
        camera.position = target_pos;
        camera.up = Vector3Normalize(target_up);
        camera.target = center;
    }

    void render() {
        // Renderiza apenas Grade e Comida primeiro
        m_grid.foreach_cell([&](CubeType t, GridPos p) {
            if (t == CubeType::SnakeBody || t == CubeType::SnakeHead) return; // Pula a cobra lógica
            render_cell(t, p);
        });
        float progress = std::clamp(m_counter / m_current_tick, 0.0f, 1.0f);
        render_snake_smoothly(progress);
    }

    bool has_lost() const { return m_lost; }
    size_t score() const { return m_snake.body().size() - Settings::SNAKE_INITIAL_LENGTH; }

private:
    void render_snake_smoothly(float progress) {
        using namespace Settings;
        const auto& current = m_snake.body();
        const auto& previous = m_snake.previous_body();
        for (size_t i = 0; i < current.size(); i++) {
            GridPos cur_pos = current[i];
            GridPos prev_pos = (i < previous.size()) ? previous[i] : current[i]; // Trata crescimento da cauda
            Vector3 visual_pos = get_interpolated_pos(cur_pos, prev_pos, progress);
            Color color = (i == 0) ? GREEN : DARKGREEN;

            DrawCube(visual_pos, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, color);
        }
    }
    void render_cell(CubeType type, GridPos position) {
        using namespace Settings;
        if (type == CubeType::None) return;
        Vector3 base { get_world_position(position) };
        if (type == CubeType::Wall) {
            DrawCube(base, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, Fade(BLUE, 0.4f));
            return;
        }
        if (type == CubeType::Food) {
            Vector3 offsetPosition = Vector3Add(base, surface_normal(position));
            DrawCube(offsetPosition, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, RED);
        }
    }
    void make_tick() {
        auto result { m_snake.make_step() };
        auto type { m_grid.at(result.position) };
        auto ate_food { type == CubeType::Food };

        // Se o próximo passo é exatamente onde a cauda está agora, e a cauda vai
        // se mover nesse mesmo tick (não cresceu), isso NÃO é colisão — é o
        // comportamento normal de "seguir o próprio rastro" do jogo da cobrinha.
        bool moving_into_tail { !ate_food && result.position == m_snake.tail() };
        bool hit_self { (type == CubeType::SnakeBody || type == CubeType::SnakeHead) && !moving_into_tail };

        if (hit_self) {
            m_lost = true;
            return;
        }

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
        m_current_tick = Settings::TICK_SECONDS_BASE;
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
        if(candidates.empty()) terminate();

        GridPos random_candidate {};
        sample(candidates.begin(), candidates.end(), &random_candidate, 1, generator);
        m_grid.at(random_candidate) = CubeType::Food;
    }
private:
    Snake m_snake {};
    Grid m_grid {};
    float m_counter {};
    float m_current_tick { Settings::TICK_SECONDS_BASE };
    bool m_lost {};
};

int main() {
    InitWindow(1000, 1000, "3d_snake");
    SetTargetFPS(60);
    auto camera { create_camera() };
    Game game {};
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        game.update(dt, camera);
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        game.render();
        EndMode3D();

        DrawText(TextFormat("Tamanho: %d", static_cast<int>(game.score()) + 3), 20, 20, 24, WHITE);
        if (game.has_lost()) {
            DrawText("GAME OVER - Pressione R para reiniciar", 240, 480, 28, RED);
        }

        EndDrawing();
    }
    CloseWindow();
    return 0;
}