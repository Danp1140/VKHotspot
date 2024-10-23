#include <vector>

class Scene {
public:
	Scene() = default;
	~Scene() = default;

	std::vector<cbRecTask> getDrawTasks();

private:
	Camera* camera;
	std::vector<Light*> lights;
	std::vector<Mesh*> meshes;
};
