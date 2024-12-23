#include <voxen/svc/file_service.hpp>

namespace voxen::svc
{

FileService::FileService(Config cfg) : m_cfg(std::move(cfg)) {}

FileService::~FileService() = default;

} // namespace voxen::svc
