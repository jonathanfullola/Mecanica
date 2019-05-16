#define GLM_SWIZZLE
#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\gtc\matrix_transform.hpp>
#include <vector>

const int MESH_COLS = 14;
const int MESH_ROWS = 18;

bool playSimulation = true;

float lambda = 5.f;
float amplitude = 0.5f;
float omega = 2.f;
glm::vec2 waveDirection(glm::normalize(glm::vec2(1.f, 1.f)));

namespace Box {
	void drawCube();
}
namespace Axis {
	void drawAxis();
}

namespace Sphere {
	extern void updateSphere(glm::vec3 pos, float radius = 1.f);
	extern void drawSphere();
}
namespace Capsule {
	extern void updateCapsule(glm::vec3 posA, glm::vec3 posB, float radius = 1.f);
	extern void drawCapsule();
}
namespace Particles {
	extern const int maxParticles;
	extern void updateParticles(int startIdx, int count, float* array_data);
	extern void drawParticles(int startIdx, int count);
}
namespace Mesh {
	extern const int numCols;
	extern const int numRows;
	extern void updateMesh(float* array_data);
	extern void drawMesh();
}
namespace Fiber {
	extern const int numVerts;
	extern void updateFiber(float* array_data);
	extern void drawFiber();
}
namespace Cube {
	extern void updateCube(const glm::mat4& transform);
	extern void drawCube();
}

struct FluidSystem {
	glm::vec3 pos[MESH_COLS * MESH_ROWS];

	glm::vec3 getInitPos(int i, int j, float initY = 3.f) {
		return glm::vec3(-5.f + 10.f / (MESH_COLS - 1) * j, 3.f, -5.f + 10.f / (MESH_ROWS - 1) * i);
	}

	glm::vec3 getGerstnerPos(int i, int j, float accum_time = 0.f) {
		glm::vec2 x0 = getInitPos(i, j).xz;
		x0 -= waveDirection * amplitude * glm::sin(glm::dot((waveDirection * (glm::two_pi<float>() / lambda)), x0) - omega * accum_time);
		float y = getInitPos(i, j).y + amplitude * glm::cos(glm::dot((waveDirection * (glm::two_pi<float>() / lambda)), x0) - omega * accum_time);
		return glm::vec3(x0.x, y, x0.y);
	}
};

FluidSystem fluidSystem;
// Boolean variables allow to show/hide the primitives
bool renderSphere = false;
bool renderCapsule = false;
bool renderParticles = false;
bool renderMesh = true;
bool renderFiber = false;
bool renderCube = false;

//You may have to change this code
void renderPrims() {
	Box::drawCube();
	Axis::drawAxis();


	if (renderSphere)
		Sphere::drawSphere();
	if (renderCapsule)
		Capsule::drawCapsule();

	if (renderParticles) {
		int startDrawingFromParticle = 0;
		int numParticlesToDraw = Particles::maxParticles;
		Particles::drawParticles(startDrawingFromParticle, numParticlesToDraw);
	}

	if (renderMesh)
		Mesh::drawMesh();
	if (renderFiber)
		Fiber::drawFiber();
	if (renderCube)
		Cube::drawCube();
}


void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	// Do your GUI code here....
	{	
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
		ImGui::Checkbox("Play simulation", &playSimulation);
	}
	// .........................
	
	ImGui::End();

	// Example code -- ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
	bool show_test_window = false;
	if(show_test_window) {
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
		ImGui::ShowTestWindow(&show_test_window);
	}
}

void PhysicsInit() {
	// Do your initialization code here...
	// ...................................

	for (int i = 0; i < MESH_ROWS; i++) {
		for (int j = 0; j < MESH_COLS; j++) {
			fluidSystem.pos[i * MESH_COLS + j] = fluidSystem.getInitPos(i, j);
		}
	}
	Mesh::updateMesh(&fluidSystem.pos[0].x);
}

void PhysicsUpdate(float dt) {
	// Do your update code here...
	// ...........................
	static float timer = 0.f;
	if (playSimulation) {
		timer += dt;
		for (int i = 0; i < MESH_ROWS; i++) {
			for (int j = 0; j < MESH_COLS; j++) {
				fluidSystem.pos[i * MESH_COLS + j] = fluidSystem.getGerstnerPos(i, j, timer);
			}
		}
		Mesh::updateMesh(&fluidSystem.pos[0].x);
	}
}

void PhysicsCleanup() {
	// Do your cleanup code here...
	// ............................
}