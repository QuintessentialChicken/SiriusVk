//
// Created by Leon on 16/09/2025.
//

#ifndef SIRIUSVK_FSM_H
#define SIRIUSVK_FSM_H


class fsm {
public:
    enum fsm_return {
        UNHANDLED = 0,
        CONTINUE,
        EXIT,
    };
    fsm();
    virtual ~fsm() = default;

    fsm_return update();
    bool runOneIteration() noexcept;

    void setState(signed short state) noexcept;
    signed short getState() const noexcept;
protected:
    // App implements this to define where to go from which state.
    virtual fsm_return updateState(signed short state) = 0;

    bool currentStateNotYetEntered = true;
private:
    signed short currentState;
    signed short nextState;
};


#endif //SIRIUSVK_FSM_H