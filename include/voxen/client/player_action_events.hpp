#pragma once

namespace voxen::client
{

enum class PlayerActionEvent {
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
	DecreaseSpeed,
	LockChunkLoadingPoint,
	ModifyBlock,
};

} // namespace voxen::client
