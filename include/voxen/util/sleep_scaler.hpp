#pragma once

namespace voxen
{

class SleepScaler {
public:

	void sleep() { std::this_thread::sleep_for(std::chrono::milliseconds(m_cur_msec)); }
	
	void hadWork()
	{

	}

	void hadNoWork()
	{

	}

private:
	uint32_t m_min_msec = 0;
	uint32_t m_max_msec = 0;

	uint32_t m_cur_msec = 0;
};

}
