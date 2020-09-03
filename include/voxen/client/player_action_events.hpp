#pragma once

namespace voxen::client
{

enum class PlayerActionEvents {
	None,
	MoveForward,
	MoveBackward,
	MoveUp,
	MoveDown,
	MoveLeft,
	MoveRight,
	RollLeft,
	RollRight,
	PauseGame,
	IncreaseSpeed,
	DecreaseSpeed
};

}
