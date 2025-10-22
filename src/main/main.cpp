#include <cassert>
#include <iostream>
#include "core/entrypoint_engine.h"

#include "game/app.h"
//
// Created by Leon on 16/09/2025.
//
App myApp;

bool MainPrologue() {
    std::cout << "Hello World!" << std::endl;
    return true;
}

bool MainOneLoopIteration() {
    return myApp.RunOneIteration();
}

SET_APP_ENTRY_POINTS(MainPrologue, MainOneLoopIteration)

int Main() {
    assert(false && "Should never reach here, Sirius is using App style entry points");
    return 0;
}