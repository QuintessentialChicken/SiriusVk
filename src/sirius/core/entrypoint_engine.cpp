//
// Created by Leon on 16/09/2025.
//

#include "entrypoint_engine.h"

#include "../graphics/device.h"

static int g_MainReturnValue = 0;

static bool callMainOnce()
{
    g_MainReturnValue = Main();
    return false;
}

// Call Main if game does not assign Entry point
bool (*g_pProjectMainPrologue)() = nullptr;
bool (*g_pProjectMainOrDoOneLoop)() = callMainOnce;

int main() {
    if (g_pProjectMainPrologue()) {
        bool carryOn = true;
        while (carryOn) {
            carryOn = g_pProjectMainOrDoOneLoop();
        }
    }
    return 0;
}