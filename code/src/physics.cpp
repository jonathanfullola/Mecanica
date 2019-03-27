#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\gtc\matrix_transform.hpp>
#include <vector>

const int UPDATES_PER_FRAME = 10;
bool playSimulation = false;

const int NUM_FIBERS = 100;
const int VERT_PER_FIBER = 5;
float ELASTICITY = 0.8;
float FRICTION = 0.2;
float fiberDist = 1;
float structuralE = 100;
float structuralD = 5;
float bendingE = 100;
float bendingD = 5;

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

struct FiberStraw {
	glm::vec3 pos[VERT_PER_FIBER];
	glm::vec3 prevPos[VERT_PER_FIBER];
	float mass = 0.02;
};

struct FiberSystem {
	FiberStraw fibers[NUM_FIBERS];

	void InitFibers() {
		for (int i = 0; i < NUM_FIBERS; i++) {
			float x = rand() % 900 / 100.f - 4.5;
			float z = rand() % 900 / 100.f - 4.5;
			for (int j = 0; j < VERT_PER_FIBER; j++) {
				fibers[i].prevPos[j] = fibers[i].pos[j] = glm::vec3(x, (0 - fiberDist) + fiberDist * j, z);
			}
		}
	}
};

struct ForceActuator {
	bool enabled = true;
	virtual glm::vec3 computeForce(float mass, const glm::vec3& position) = 0;
};

struct DirectionalForce : ForceActuator {
	glm::vec3 forceAcceleration;
	glm::vec3 computeForce(float mass, const glm::vec3& position) {
		return forceAcceleration * mass;
	}
};

glm::vec3 springforce(const glm::vec3& P1, const glm::vec3& V1, const glm::vec3& P2, const glm::vec3& V2, float L0, float ke, float kd) {
	return -(ke * (glm::length(P1 - P2) - L0) + kd * glm::dot((V1 - V2), glm::normalize(P1 - P2)))* glm::normalize(P1 - P2);
}

glm::vec3 computeForces(FiberStraw& fiber, int idx, const std::vector<ForceActuator*>& force_acts) {
	glm::vec3 force = { 0, 0, 0 };
	for (int i = 0; i < force_acts.size(); i++) {
		force += force_acts[i]->computeForce(fiber.mass, fiber.pos[idx]);
	}
	return force;
}

struct Collider {
	bool enabled = true;
	glm::vec3 normalPlane;
	float dPlane;
	virtual bool checkCollision(const glm::vec3& prev_pos, const glm::vec3& next_pos) = 0;
	virtual void getPlane(glm::vec3& normal, float& d) = 0;
	void computeCollision(glm::vec3& old_pos, glm::vec3& new_pos) {
		if (enabled) {
			if (!checkCollision(old_pos, new_pos))
				return;
			glm::vec3 normal;
			float d;
			getPlane(normal, d);
			new_pos -= (1 + ELASTICITY) * (glm::dot(normal, new_pos) + d) * normal;
			old_pos -= (1 + ELASTICITY) * (glm::dot(normal, old_pos) + d) * normal;

			glm::vec3 frictionDist = ((new_pos - ((2 * (glm::dot(normal, new_pos) + d)) * normal)) - old_pos) / 2.f * FRICTION;
			old_pos += frictionDist;
			new_pos -= frictionDist;
		}
	}
};

struct PlaneCol : Collider {
	PlaneCol(glm::vec3 normal_, glm::vec3 point) {
		normalPlane = glm::normalize(normal_);
		dPlane = (normalPlane.x * point.x + normalPlane.y * point.y + normalPlane.z * point.z) * -1;
	}
	bool checkCollision(const glm::vec3& prev_pos, const glm::vec3& next_pos) {
		return /*(glm::dot(normalPlane, prev_pos) + dPlane) **/ (glm::dot(normalPlane, next_pos) + dPlane) < 0.01;
	}
	void getPlane(glm::vec3& normal, float& d) {
		normal = normalPlane;
		d = dPlane;
	}
};

struct SphereCol : Collider {
	glm::vec3 center;
	float radius;
	bool checkCollision(const glm::vec3& prev_pos, const glm::vec3& next_pos) {
		if ((glm::length(next_pos - center) - radius) <= 0)
		{
			glm::vec3 vec = next_pos - prev_pos;

			float a = ((pow(vec.x, 2) + pow(vec.y, 2) + pow(vec.z, 2)));
			float b = -(2 * (center.x*vec.x + center.y*vec.y + center.z*vec.z - prev_pos.x*vec.x - prev_pos.y*vec.y - prev_pos.z*vec.z));
			float c = ((pow(center.x, 2) + pow(center.y, 2) + pow(center.z, 2) - 2 * (center.x * prev_pos.x + center.y * prev_pos.y
				+ center.z * prev_pos.z) + pow(prev_pos.x, 2) + pow(prev_pos.y, 2) + pow(prev_pos.z, 2) - glm::pow(radius, 2)));

			float lambda1 = (-b + glm::sqrt(glm::pow(b, 2) - (4 * a * c))) / (2 * a);
			float lambda2 = (-b - glm::sqrt(glm::pow(b, 2) - (4 * a * c))) / (2 * a);

			glm::vec3 colPoint1 = { prev_pos.x + lambda1 * vec.x, prev_pos.y + lambda1 * vec.y, prev_pos.z + lambda1 * vec.z, };
			glm::vec3 colPoint2 = { prev_pos.x + lambda2 * vec.x, prev_pos.y + lambda2 * vec.y, prev_pos.z + lambda2 * vec.z, };

			glm::vec3 colPoint = (distance(prev_pos, colPoint1) < distance(prev_pos, colPoint2)) ? colPoint1 : colPoint2;

			normalPlane = glm::normalize(colPoint - center);

			dPlane = -(normalPlane.x * colPoint.x + normalPlane.y * colPoint.y + normalPlane.z * colPoint.z);
			return true;
		}
		return false;
	}
	void getPlane(glm::vec3& normal, float& d) {
		normal = normalPlane;
		d = dPlane;
	}
};

FiberSystem fiberSystem;
std::vector<ForceActuator*> forceArray;
std::vector<Collider*> colliderArray;

void verlet(float dt, FiberStraw& fiber, const std::vector<Collider*>& colliders, const std::vector<ForceActuator*>& force_acts) {
	for (int i = 0; i < UPDATES_PER_FRAME; i++) {
		glm::vec3 forces[VERT_PER_FIBER];

		for (int i = 1; i < VERT_PER_FIBER; i++) {
			forces[i] = computeForces(fiber, i, force_acts);
		}

		for (int i = 0; i < VERT_PER_FIBER - 1; i++) {
			glm::vec3 spring = springforce(fiber.pos[i], (fiber.pos[i] - fiber.prevPos[i]) / 2.f, fiber.pos[i + 1], (fiber.pos[i + 1] - fiber.prevPos[i + 1]) / (dt / UPDATES_PER_FRAME), fiberDist, structuralE, structuralD);
			forces[i] += spring;
			forces[i + 1] -= spring;
		}

		for (int i = 0; i < VERT_PER_FIBER - 2; i++) {
			glm::vec3 spring = springforce(fiber.pos[i], (fiber.pos[i] - fiber.prevPos[i]) / 2.f, fiber.pos[i + 2], (fiber.pos[i + 2] - fiber.prevPos[i + 2]) / (dt / UPDATES_PER_FRAME), fiberDist * 2, bendingE, bendingD);
			forces[i] += spring;
			forces[i + 2] -= spring;
		}

		forces[0] = { 0, 0, 0 };

		for (int i = 0; i < VERT_PER_FIBER; i++) {
			glm::vec3 temp = fiber.pos[i];
			fiber.pos[i] = fiber.pos[i] + (fiber.pos[i] - fiber.prevPos[i]) + forces[i] / fiber.mass * glm::pow((dt / UPDATES_PER_FRAME), 2);
			fiber.prevPos[i] = temp;
		}

		for (int i = 1; i < VERT_PER_FIBER; i++) {
			for (int j = 0; j < colliders.size(); j++) {
				colliders[j]->computeCollision(fiber.prevPos[i], fiber.pos[i]);
			}
		}
	}
}

SphereCol ball;
DirectionalForce gravity;
DirectionalForce wind;

// Boolean variables allow to show/hide the primitives
bool renderSphere = true;
bool renderCapsule = false;
bool renderParticles = false;
bool renderMesh = false;
bool renderFiber = true;
bool renderCube = false;

//You may have to change this code
void renderPrims() {
	Box::drawCube();
	Axis::drawAxis();


	if (renderSphere) {
		Sphere::updateSphere(ball.center, ball.radius);
		Sphere::drawSphere();
	}
	if (renderCapsule)
		Capsule::drawCapsule();

	if (renderParticles) {
		int startDrawingFromParticle = 0;
		int numParticlesToDraw = Particles::maxParticles;
		Particles::drawParticles(startDrawingFromParticle, numParticlesToDraw);
	}

	if (renderMesh)
		Mesh::drawMesh();
	if (renderFiber) {
		for (int i = 0; i < NUM_FIBERS; i++) {
			Fiber::updateFiber(&fiberSystem.fibers[i].pos[0].x);
			Fiber::drawFiber();
		}
	}
		
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
		if (ImGui::Button("Reset simulation"))
			fiberSystem.InitFibers();
		if (ImGui::TreeNode("Spring parameters"))
		{
			static float structural[2] = {structuralE, structuralD};
			if (ImGui::InputFloat2("K_Structural", structural)) {
				structuralE = structural[0];
				structuralD = structural[1];
			}
			static float bending[2] = { bendingE, bendingD };
			if (ImGui::InputFloat2("K_Bending", bending)) {
				bendingE = bending[0];
				bendingD = bending[1];
			}
			if (ImGui::DragFloat("Particle link distance", &fiberDist, 0.05f)) {
				if (fiberDist < 0) { ELASTICITY = 0.f; }
			}
			ImGui::TreePop();
		}
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
	ball.center = { 0 , 5, 0 };
	ball.radius = 1.f;
	colliderArray.push_back(&ball);
	colliderArray.push_back(new PlaneCol(glm::vec3(1, 0, 0), glm::vec3(-5, 0, -5)));
	colliderArray.push_back(new PlaneCol(glm::vec3(0, 1, 0), glm::vec3(-5, 0, -5)));
	colliderArray.push_back(new PlaneCol(glm::vec3(0, 0, 1), glm::vec3(-5, 0, -5)));
	colliderArray.push_back(new PlaneCol(glm::vec3(-1, 0, 0), glm::vec3(5, 10, 5)));
	colliderArray.push_back(new PlaneCol(glm::vec3(0, -1, 0), glm::vec3(5, 10, 5)));
	colliderArray.push_back(new PlaneCol(glm::vec3(0, 0, -1), glm::vec3(5, 10, 5)));

	gravity.forceAcceleration = { 0, -0.981, 0 };
	forceArray.push_back(&gravity);
	wind.forceAcceleration = { 0.5, 0, 0 };
	forceArray.push_back(&wind);

	fiberSystem.InitFibers();
}

void PhysicsUpdate(float dt) {
	// Do your update code here...
	// ...........................
	if (playSimulation) {
		for (int i = 0; i < NUM_FIBERS; i++) {
			verlet(dt, fiberSystem.fibers[i], colliderArray, forceArray);
		}
	}
}

void PhysicsCleanup() {
	// Do your cleanup code here...
	// ............................
}