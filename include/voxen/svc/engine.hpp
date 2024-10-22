#pragma once

#include <voxen/svc/service_locator.hpp>
#include <voxen/visibility.hpp>

#include <memory>

namespace voxen::svc
{

class VOXEN_API Engine {
public:
	Engine(Engine &&) = delete;
	Engine(const Engine &) = delete;
	Engine &operator=(Engine &&) = delete;
	Engine &operator=(const Engine &) = delete;
	~Engine() noexcept = default;

	static std::unique_ptr<Engine> create();

private:
	Engine();

	ServiceLocator m_service_locator;
};

} // namespace voxen::svc
