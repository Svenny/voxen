#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/visibility.hpp>

namespace voxen::svc
{

// Forward-declared here - this declaration is used by virtually any service
class ServiceLocator;

class VOXEN_API IService {
public:
	IService() = default;
	IService(IService &&) = delete;
	IService(const IService &) = delete;
	IService &operator=(IService &&) = delete;
	IService &operator=(const IService &) = delete;
	virtual ~IService() noexcept;

	virtual UID serviceUid() const noexcept = 0;
};

template<typename T>
concept CService = std::is_base_of_v<IService, T> && requires {
	{ T::SERVICE_UID } -> std::same_as<const UID &>;
};

} // namespace voxen::svc
