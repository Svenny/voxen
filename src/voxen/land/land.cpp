#if 0
namespace voxen::land
{

namespace
{

class TestBlock final : public IBlock {
public:
	~TestBlock() override = default;

	std::string_view getInternalName() const noexcept override { return "voxen/test_block"; }

	PackedColorLinear getImpostorColor() const noexcept override { return PackedColorLinear(0x02, 0x2F, 0x8E); }
};

} // namespace

Land::Land(svc::ServiceLocator &svc) : m_impl(std::make_unique<Impl>(svc.requestService<ThreadPool>()))
{
	uint16_t id = m_impl->block_registry.registerBlock(std::make_shared<TestBlock>());
	(void) id;
	// We've just created the registry
	assert(id == 1);
}


} // namespace voxen::land
#endif
