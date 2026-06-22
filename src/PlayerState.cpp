#include "PlayerState.h"
#include "globals.h"

PlayerStateMachine::PlayerStateMachine()
    : m_currentState(PlayerState::IDLE)
    , m_previousState(PlayerState::IDLE)
    , m_attackTimer(0.0f)
    , m_defendTimer(0.0f)
{
}

int PlayerStateMachine::GetAnimationIndex() const {
    switch (m_currentState) {
        case PlayerState::WALKING:          return 1;
        case PlayerState::IDLE:             return 2;
        case PlayerState::RUNNING:          return 3;
        case PlayerState::CLIMBING_VINE:    return 2;
        case PlayerState::CLIMBING_LADDER:  return 2;
        case PlayerState::ATTACKING:        return 6 + m_attackVariant;
        case PlayerState::DEFENDING:        return 5;
        case PlayerState::JUMPING:          return 6;
    }
    return 2;
}

float PlayerStateMachine::GetMovementSpeed() const {
    switch (m_currentState) {
        case PlayerState::RUNNING:    return m_runSpeed * m_runSpeedMultiplier;
        case PlayerState::WALKING:    return m_walkSpeed;
        case PlayerState::DEFENDING:  return m_walkSpeed * 0.43f;
        case PlayerState::ATTACKING:  return 0.0f;
        case PlayerState::JUMPING:    return m_jumpSpeed;
        default: return 0.0f;
    }
}

void PlayerStateMachine::Update(bool wPressed, bool sPressed, bool shiftPressed,
                                bool attackPressed, bool defendPressed, bool jumpPressed,
                                bool isClimbingVine, bool isClimbingLadder,
                                bool onGround, float deltaTime)
{
    m_previousState = m_currentState;

    bool wantsMove = wPressed || sPressed;

    if (isClimbingLadder) {
        m_currentState = PlayerState::CLIMBING_LADDER;
        return;
    }
    if (isClimbingVine) {
        m_currentState = PlayerState::CLIMBING_VINE;
        return;
    }

    if (m_currentState == PlayerState::JUMPING) {
        if (!onGround) m_wasAirborne = true;
        if (onGround && m_wasAirborne) {
            m_currentState = wantsMove ? PlayerState::WALKING : PlayerState::IDLE;
            m_wasAirborne = false;
        }
        return;
    }

    if (jumpPressed && onGround && m_currentState != PlayerState::JUMPING) {
        bool wasRunning = (m_currentState == PlayerState::RUNNING);
        m_currentState = PlayerState::JUMPING;
        m_wasAirborne = false;
        m_jumpSpeed = wasRunning ? (m_runSpeed * m_runSpeedMultiplier) : m_walkSpeed;
        return;
    }

    if (attackPressed && m_currentState != PlayerState::ATTACKING) {
        m_currentState = PlayerState::ATTACKING;
        m_attackTimer = ATTACK_DURATION;
        return;
    }

    if (m_currentState == PlayerState::ATTACKING) {
        m_attackTimer -= deltaTime;
        if (m_attackTimer <= 0.0f) {
            m_currentState = wantsMove ? PlayerState::WALKING : PlayerState::IDLE;
        }
        return;
    }

    if (defendPressed) {
        m_currentState = PlayerState::DEFENDING;
        return;
    }

    if (m_currentState == PlayerState::DEFENDING) {
        m_currentState = wantsMove ? PlayerState::WALKING : PlayerState::IDLE;
        return;
    }

    if (wantsMove) {
        if (shiftPressed && wPressed && !sPressed) {
            m_currentState = PlayerState::RUNNING;
        } else {
            m_currentState = PlayerState::WALKING;
        }
    } else {
        m_currentState = PlayerState::IDLE;
    }
}
