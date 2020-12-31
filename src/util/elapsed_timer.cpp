#include <voxen/util/elapsed_timer.hpp>

#include <voxen/util/log.hpp>

#include <cassert>

namespace voxen
{

ElapsedTimer::ElapsedTimer(std::string section_name, std::string format): m_section_name(std::move(section_name)), m_format(std::move(format))
{
    // ElapsedTimer for dev propouse only, so this shouldn't be used in Deploy Builds
    if constexpr (BuildConfig::kIsDeployBuild) {
        Log::fatal("Elapsed timers mustn't used in deploy builds, but timer for \"{}\" have been created", m_section_name);
        assert(false);
    }

    m_start = std::chrono::high_resolution_clock::now();
}

ElapsedTimer::~ElapsedTimer() noexcept
{
    if (!m_finished) {
        Log::warn("Elapsed timer for section \"{}\" destroyed without stop!", m_section_name);
    }
}

void ElapsedTimer::stop()
{
    m_end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = m_end - m_start;
    std::chrono::milliseconds elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
    Log::debug(m_format, m_section_name, elapsed_ms.count());

    m_finished = true;
}

}
