#pragma once

#include <fmt/format.h>

#include <chrono>

namespace voxen
{

/**
 * This is class is voxen-specific class for code segment measurements.
 * The class will collect time point on constructor/stop method and print them into log (Debug level) automatically.
 * Should be used like this:
 *    ElapsedTimer test_timer("section name");
 *    <measurable code here>
 *    test_timer.stop()
 */
class ElapsedTimer {
public:
	ElapsedTimer(std::string section_name,
		std::string format = "[Elapsed timer] execution of section \"{}\" tooks {} ms");
	ElapsedTimer(const ElapsedTimer& other) = delete;
	ElapsedTimer(ElapsedTimer&& other) = delete;
	~ElapsedTimer() noexcept;

	ElapsedTimer& operator=(const ElapsedTimer& other) = delete;
	ElapsedTimer& operator=(ElapsedTimer&& other) = delete;

	void stop();

private:
	const std::string m_section_name;
	const std::string m_format;

	bool m_finished = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_end;
};

} // namespace voxen
