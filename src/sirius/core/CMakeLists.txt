add_library(core STATIC
        fsm.cpp
        fsm.h
        entrypoint_engine.cpp
        entrypoint_engine.h
        types.h
        utils.cpp
        utils.h
)

target_link_libraries(core PRIVATE fmt::fmt)