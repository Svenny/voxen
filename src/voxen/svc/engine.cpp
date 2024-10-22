#include <voxen/svc/engine.hpp>

namespace voxen::svc
{

Engine::Engine()
{
	//m_service_locator.registerServiceFactory()
}

std::unique_ptr<Engine> Engine::create()
{
	return std::unique_ptr<Engine>(new Engine);
}

}
