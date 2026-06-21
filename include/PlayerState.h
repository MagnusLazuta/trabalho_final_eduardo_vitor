#ifndef PLAYER_STATE_H
#define PLAYER_STATE_H

enum class PlayerState {
    IDLE,
    WALKING,
    RUNNING,
    CLIMBING_VINE,
    CLIMBING_LADDER,
    ATTACKING,
    DEFENDING,
    JUMPING
};

class PlayerStateMachine {
public:
    PlayerStateMachine();

    void Update(bool wPressed, bool sPressed, bool shiftPressed,
                bool attackPressed, bool defendPressed, bool jumpPressed,
                bool isClimbingVine, bool isClimbingLadder,
                bool onGround, float deltaTime);

    PlayerState GetCurrentState() const { return m_currentState; }
    PlayerState GetPreviousState() const { return m_previousState; }
    int GetAnimationIndex() const;
    float GetMovementSpeed() const;
    bool IsAttacking() const { return m_currentState == PlayerState::ATTACKING; }
    bool IsDefending() const { return m_currentState == PlayerState::DEFENDING; }
    bool IsJumping()    const { return m_currentState == PlayerState::JUMPING; }

    void SetWalkSpeed(float speed) { m_walkSpeed = speed; }
    void SetRunSpeed(float speed)  { m_runSpeed  = speed; }
    float GetWalkSpeed() const { return m_walkSpeed; }
    float GetRunSpeed()  const { return m_runSpeed; }

private:
    PlayerState m_currentState;
    PlayerState m_previousState;
    float m_attackTimer;
    float m_defendTimer;
    bool  m_wasAirborne = false;
    float m_jumpSpeed = 0.0f;

    float m_walkSpeed = 3.5f;
    float m_runSpeed  = 6.0f;
    float m_runSpeedMultiplier = 1.8f;

    static constexpr float ATTACK_DURATION = 1.0f;
    static constexpr float DEFEND_DURATION = 1.0f;
};

#endif
