#include <cassert>
#include <iostream>
#include "core/entrypoint_engine.h"

#include "game/app.h"
//
// Created by Leon on 16/09/2025.
//
app my_app;

bool Main_Prologue() {
    std::cout << "Hello World!" << std::endl;
    return true;
}

bool Main_OneLoopIteration() {
    return my_app.runOneIteration();
}

SET_APP_ENTRY_POINTS(Main_Prologue, Main_OneLoopIteration)

int Main() {
    assert(false && "Should never reach here, Sirius is using App style entry points");
    return 0;
}