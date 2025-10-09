//
// Created by Leon on 16/09/2025.
//

#pragma once

#include "core/fsm.h"
#include "graphics/vkRenderer.h"

class app : public Fsm {
public:

    enum {
        INIT_SYSTEM = 0,
        RUN_GAME,
        SHUTDOWN_SYSTEM
    };

    FsmReturn UpdateState(signed short state) override;
    // Implement state machine here later to handle Initialization, Game loop and teardown
    // App can inherit from state machine class, RunOneIteration is part of state machine class
    FsmReturn init();

    FsmReturn shutdown();

    FsmReturn runGame();
private:
    bool isSystemInitialized = false;
};


