#include <voxen/util/error_condition.hpp>

namespace voxen
{

namespace
{

struct VoxenErrorCategory : std::error_category {
	const char *name() const noexcept override
	{
		return "Voxen error";
	}

	std::string message(int code) const override
	{
		switch (static_cast<VoxenErrc>(code)) {
		case VoxenErrc::GfxFailure: return "Error happened in graphics subsystem";
		case VoxenErrc::GfxCapabilityMissing: return "Graphics subsystem does not have the required capability";
		case VoxenErrc::FileNotFound: return "Requested file does not exist or is inaccessible";
		case VoxenErrc::InvalidData: return "Input data is invalid/corrupt and can't be used";
		case VoxenErrc::OutOfResource: return "A finite resource was exhausted";
		case VoxenErrc::OptionMissing: return "A config object has no requested option but user assumes it exists";
		case VoxenErrc::DataTooLarge: return "Input data exceeds the processible limit";
		case VoxenErrc::ExternalLibFailure: return "Call to external library failed for library-specific reasons";
		// No `default` to make `-Werror -Wswitch` protection work
		}

		return "Unknown error";
	}
};

const VoxenErrorCategory g_category;

} // anonymous namespace

std::error_condition make_error_condition(VoxenErrc errc) noexcept
{
	return { static_cast<int>(errc), g_category };
}

}
