#include <raylib.h>
#include <cstddef>


namespace Settings{
  constexpr std::size_t GRIDE_SIZE { 10 };
  constexpr float CUBE_SIZE { .5f };
  constexpr float CUBE_SPACING { .15f };
  constexpr float TICK_SECONDS { .3f };
}


Camera3D create_camera(){
            Camera3D camera {};
            

            camera.position = {10, 10, 10};
            camera.fovy = 60;
            camera.up = { 0, 1, 0 };
            camera.projection = CAMERA_PERSPECTIVE;

            return camera;
}

class Game{
  public:
      void update(float dt){
        m_counter += dt;
        if(m_counter > Settings::TICK_SECONDS)
      }

  private:
};

int main(){

        InitWindow(1000, 1000, "3d_snake");
        
        auto camera {create_camera()}; 

        while(!WindowShouldClose()){
          BeginDrawing();
          ClearBackground(BLACK);
          BeginMode3D(camera);
          DrawCube({}, 1, 2, 1, BLUE);
          EndMode3D();
          EndDrawing();
    }
  
  


  return 0;
}
