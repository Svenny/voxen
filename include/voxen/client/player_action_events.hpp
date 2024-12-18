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

	ChooseItem0,
	ChooseItem1 = ChooseItem0 + 1,
	ChooseItem2 = ChooseItem0 + 2,
	ChooseItem3 = ChooseItem0 + 3,
	ChooseItem4 = ChooseItem0 + 4,
	ChooseItem5 = ChooseItem0 + 5,
	ChooseItem6 = ChooseItem0 + 6,
	ChooseItem7 = ChooseItem0 + 7,
	ChooseItem8 = ChooseItem0 + 8,
	ChooseItem9 = ChooseItem0 + 9,
};

} // namespace voxen::client
