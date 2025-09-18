//
// Created by Leon on 16/09/2025.
//

#pragma once

#include "core/fsm.h"
#include "graphics/renderer.h"

class app : public fsm {
public:

    enum {
        INIT_SYSTEM = 0,
        RUN_GAME,
        SHUTDOWN_SYSTEM
    };

    fsm_return updateState(signed short state) override;
    // Implement state machine here later to handle Initialization, Game loop and teardown
    // App can inherit from state machine class, RunOneIteration is part of state machine class
    fsm_return init();

    fsm_return shutdown();

    fsm_return runGame();
private:
    std::unique_ptr<sirius::srsRenderer> renderer;
    bool isSystemInitialized = false;
};


