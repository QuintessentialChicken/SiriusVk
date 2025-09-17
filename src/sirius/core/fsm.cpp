//
// Created by Leon on 16/09/2025.
//

#include "fsm.h"

fsm::fsm() {
    currentState = 0;
    nextState = 0;
}

fsm::fsm_return fsm::update() {
    fsm_return ret = UNHANDLED;
    while (ret == UNHANDLED) {
        // If the next state differs from the current state, there is a request to transition to the next state
        // Update the current state and let the app handle it
        if (currentState != nextState) {
            currentState = nextState;
            ret = updateState(currentState);
            // If the next state is the same as the current, we might be in a scenario where the state hasn't been updated yet but we also weren't in the current state before. This occurs in the very first run for example.
            // Check if we enter for the first time (we could send an event or different state to allow for some setup of the state)
            // If yes, clear the flag. If not, do a regular update. This probably leads to the main game loop
        } else {
            if (currentStateNotYetEntered) {
                currentStateNotYetEntered = false;
                ret = updateState(currentState);
            } else {
                ret = updateState(currentState);
            }
        }
    }
    return ret;
}

bool fsm::runOneIteration() noexcept {
    if (update() == EXIT) {
        return false;
    }
    return true;
}

void fsm::setState(signed short state) noexcept {
    nextState = state;
}

signed short fsm::getState() const noexcept {
    return currentState;
}
