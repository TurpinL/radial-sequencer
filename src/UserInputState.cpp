#include "UserInputState.hpp"

UserInputState::UserInputState(SineCosinePot *endlessPot, std::vector<Button*> &activeButtons) {
    _endlessPot = endlessPot;
    _activeButtons = activeButtons;

    _baseButton = activeButtons.size() > 0 ? activeButtons[0] : nullptr;
    _baseCommand = _baseButton != nullptr ? _baseButton->_command : NOTHING;
  
    _modifierButton = activeButtons.size() > 1 ? activeButtons[1] : nullptr;
    _modifierCommand = _modifierButton != nullptr ? _modifierButton->_command : NOTHING;
}