/*
 * The fireworks demo.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cyclone/cyclone.h>
#include "../ogl_headers.h"
#include "../app.h"
#include "../timing.h"

#include <stdio.h>

static cyclone::Random crandom;

/**
 * 불꽃놀이는 입자 시스템에, 계산과 렌더링에 필요한 몇 가지 데이터를 추가하여 구성한다.
 */
class Firework : public cyclone::Particle
{
public:
    /** 불꽃 입자의 계산 규칙을 정하는 정수 값 */
    unsigned type;

    /**
     * 입자의 수명을 계산해 언제 폭발할지를 정한다. 수명은 조금씩 감소시킨다.
     * 수명이 0이되면 화약을 터뜨린다. 수명을 심지의 남은 시간으로 생각해본다.
     */
    cyclone::real age;

    /**
     * 주어진 시간 간격에 대하여 불꽃놀이를 업데이트 한다. 
     * 불꽃이 그 수명을 다해서 소멸시켜야 한다면 true를 반환한다.
     */
    bool update(cyclone::real duration)
    {
        // Update our physical state
        // 물리적 상태를 업데이트 한다.
        integrate(duration);

        // We work backwards from our age to zero.
        // age를 0이 될 때까지 감소시킨다.
        age -= duration;
        return (age < 0) || (position.y < 0);
    }
};

/**
 * 불꽃놀이 규칙에 의해 심지의 지속 시간과 어떤 입자로 바뀌는지를 제어한다.
 */
struct FireworkRule
{
    /** 이 규칙이 적용되는 불꽃의 종류 */
    unsigned type;

    /** 심지의 최소 지속 시간 */
    cyclone::real minAge;

    /** 심지의 최대 지속 시간 */
    cyclone::real maxAge;

    /** 불꽃 입자의 최소 상대 속도 */
    cyclone::Vector3 minVelocity;

    /** 불꽃 입자의 최대 상대 속도 */
    cyclone::Vector3 maxVelocity;

    /** 불꽃의 댐필 계수 */
    cyclone::real damping;

    /**
     * 페이로드는 심지가 다 타서 화약이 폭발하면 발생할 새로운 불꽃이다.
     */
    struct Payload
    {
        /** 생성할 새로운 입자의 종류 */
        unsigned type;

        /** 이 페이로드의 입자의 개수 */
        unsigned count;

        /** 페이로드의 속성 설정 */
        void set(unsigned type, unsigned count)
        {
            Payload::type = type;
            Payload::count = count;
        }
    };

    /** 이 불꽃 종류에 대한 페이로드의 수 */
    unsigned payloadCount;

    /** 페이로드의 집합 */
    Payload *payloads;

    FireworkRule()
    :
    payloadCount(0),
    payloads(NULL)
    {
    }

    void init(unsigned payloadCount)
    {
        FireworkRule::payloadCount = payloadCount;
        payloads = new Payload[payloadCount];
    }

    ~FireworkRule()
    {
        if (payloads != NULL) delete[] payloads;
    }

    /**
     * Set all the rule parameters in one go.
     */
    void setParameters(unsigned type, cyclone::real minAge, cyclone::real maxAge,
        const cyclone::Vector3 &minVelocity, const cyclone::Vector3 &maxVelocity,
        cyclone::real damping)
    {
        FireworkRule::type = type;
        FireworkRule::minAge = minAge;
        FireworkRule::maxAge = maxAge;
        FireworkRule::minVelocity = minVelocity;
        FireworkRule::maxVelocity = maxVelocity;
        FireworkRule::damping = damping;
    }

    /**
     * 새로운 불꽃을 생성하여 주어진 인스턴스에 이를 기록한다. 새로운 불꽃의
     * 위치와 초속도는 부모 불꽃의 것을 가져다 쓴다.
     */
    void create(Firework *firework, const Firework *parent = NULL) const
    {
        firework->type = type;
        firework->age = crandom.randomReal(minAge, maxAge);

        cyclone::Vector3 vel;
        if (parent) {
            // 위치와 속도는 부모의 것을 가져다 쓴다.
            firework->setPosition(parent->getPosition());
            vel += parent->getVelocity();
        }
        else
        {
            cyclone::Vector3 start;
            int x = (int)crandom.randomInt(3) - 1;
            start.x = 5.0f * cyclone::real(x);
            firework->setPosition(start);
        }

        vel += crandom.randomVector(minVelocity, maxVelocity);
        firework->setVelocity(vel);

        // We use a mass of one in all cases (no point having fireworks
        // with different masses, since they are only under the influence
        // of gravity).

        // 입자의 질량은 모두 1로 설정한다.
        // 입자가 받는 힘은 중력뿐이다.
        firework->setMass(1);

        firework->setDamping(damping);

        firework->setAcceleration(cyclone::Vector3::GRAVITY);

        firework->clearAccumulator();
    }
};

/**
 * The main demo class definition.
 */
class FireworksDemo : public Application
{
    /**
     * Holds the maximum number of fireworks that can be in use.
     */
    const static unsigned maxFireworks = 1024;

    /** Holds the firework data. */
    Firework fireworks[maxFireworks];

    /** Holds the index of the next firework slot to use. */
    unsigned nextFirework;

    /** And the number of rules. */
    const static unsigned ruleCount = 30;

    /** Holds the set of rules. */
    FireworkRule rules[ruleCount];

    /** Dispatches a firework from the origin. */
    void create(unsigned type, const Firework *parent=NULL);

    /** Dispatches the given number of fireworks from the given parent. */
    void create(unsigned type, unsigned number, const Firework *parent);

    /** Creates the rules. */
    void initFireworkRules();

public:
    /** Creates a new demo object. */
    FireworksDemo();
    ~FireworksDemo();

    /** Sets up the graphic rendering. */
    virtual void initGraphics();

    /** Returns the window title for the demo. */
    virtual const char* getTitle();

    /** Update the particle positions. */
    virtual void update();

    /** Display the particle positions. */
    virtual void display();

    /** Handle a keypress. */
    virtual void key(unsigned char key);
    virtual void specialkey(unsigned char key);

    cyclone::Vector3 CameraPos;
    cyclone::Quaternion CameraRot;
    
};

// Method definitions
FireworksDemo::FireworksDemo()
:
nextFirework(0)
{
    // Make all shots unused
    for (Firework *firework = fireworks;
         firework < fireworks+maxFireworks;
         firework++)
    {
        firework->type = 0;
    }

    // Create the firework types
    initFireworkRules();
}

FireworksDemo::~FireworksDemo()
{
}

void FireworksDemo::initFireworkRules()
{
    // Go through the firework types and create their rules.
    rules[0].init(2);
    rules[0].setParameters(
        1, // type
        0.5f, 1.4f, // age range
        cyclone::Vector3(-5, 25, -5), // min velocity
        cyclone::Vector3(5, 28, 5), // max velocity
        0.1 // damping
        );
    rules[0].payloads[0].set(3, 5);
    rules[0].payloads[1].set(5, 5);

    rules[1].init(1);
    rules[1].setParameters(
        2, // type
        0.5f, 1.0f, // age range
        cyclone::Vector3(-5, 10, -5), // min velocity
        cyclone::Vector3(5, 20, 5), // max velocity
        0.8 // damping
        );
    rules[1].payloads[0].set(4, 2);

    rules[2].init(0);
    rules[2].setParameters(
        3, // type
        0.5f, 1.5f, // age range
        cyclone::Vector3(-5, -5, -5), // min velocity
        cyclone::Vector3(5, 5, 5), // max velocity
        0.1 // damping
        );

    rules[3].init(0);
    rules[3].setParameters(
        4, // type
        0.25f, 0.5f, // age range
        cyclone::Vector3(-20, 5, -5), // min velocity
        cyclone::Vector3(20, 5, 5), // max velocity
        0.2 // damping
        );

    rules[4].init(1);
    rules[4].setParameters(
        5, // type
        0.5f, 1.0f, // age range
        cyclone::Vector3(-20, 2, -5), // min velocity
        cyclone::Vector3(20, 18, 5), // max velocity
        0.01 // damping
        );
    rules[4].payloads[0].set(3, 5);

    rules[5].init(0);
    rules[5].setParameters(
        6, // type
        3, 5, // age range
        cyclone::Vector3(-5, 5, -5), // min velocity
        cyclone::Vector3(5, 10, 5), // max velocity
        0.95 // damping
        );

    rules[6].init(1);
    rules[6].setParameters(
        7, // type
        4, 5, // age range
        cyclone::Vector3(-5, 50, -5), // min velocity
        cyclone::Vector3(5, 60, 5), // max velocity
        0.01 // damping
        );
    rules[6].payloads[0].set(8, 10);

    rules[7].init(0);
    rules[7].setParameters(
        8, // type
        0.25f, 0.5f, // age range
        cyclone::Vector3(-1, -1, -1), // min velocity
        cyclone::Vector3(1, 1, 1), // max velocity
        0.01 // damping
        );

    rules[8].init(0);
    rules[8].setParameters(
        9, // type
        3, 5, // age range
        cyclone::Vector3(-15, 10, -5), // min velocity
        cyclone::Vector3(15, 15, 5), // max velocity
        0.95 // damping
        );


    rules[9].init(3);
    rules[9].setParameters(
        10, // type
        3, 5, // age range
        cyclone::Vector3(-15, 10, -5), // min velocity
        cyclone::Vector3(15, 15, 5), // max velocity
        0.95 // damping
        );

    rules[9].payloads[0].set(3, 5);
    rules[9].payloads[1].set(5, 5);
    rules[9].payloads[2].set(1, 5);

    // ... and so on for other firework types ...
}

void FireworksDemo::initGraphics()
{
    // Call the superclass
    Application::initGraphics();

    // But override the clear color
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
}

const char* FireworksDemo::getTitle()
{
    return "Cyclone > Fireworks Demo";
}

void FireworksDemo::create(unsigned type, const Firework *parent)
{
    // Get the rule needed to create this firework
    // 불꽃을 생성하는 규칙을 받아온다.
    FireworkRule *rule = rules + (type - 1);

    // Create the firework
    // 불꽃을 생성한다.
    rule->create(fireworks+nextFirework, parent);

    // Increment the index for the next firework
    // 다음 불꽃을 위해 인덱스를 하나 증가시킨다.
    nextFirework = (nextFirework + 1) % maxFireworks;
}

void FireworksDemo::create(unsigned type, unsigned number, const Firework *parent)
{
    for (unsigned i = 0; i < number; i++)
    {
        create(type, parent);
    }
}

void FireworksDemo::update()
{
    // Find the duration of the last frame in seconds
    float duration = (float)TimingData::get().lastFrameDuration * 0.001f;
    if (duration <= 0.0f) return;

    for (Firework *firework = fireworks; firework < fireworks+maxFireworks; firework++)
    {
        // Check if we need to process this firework.
        // 이 불꽃을 처리해야 하는지를 검사한다
        if (firework->type > 0)
        {
            // Does it need removing?
            // 제거해야 하는가?
            if (firework->update(duration))
            {
                // Find the appropriate rule
                // 적절한 규칙을 찾는다.
                FireworkRule *rule = rules + (firework->type-1);

                // Delete the current firework (this doesn't affect its
                // position and velocity for passing to the create function,
                // just whether or not it is processed for rendering or
                // physics.
                // 현재 불꽃을 제거한다.
                // (생성 함수에 정달하기 위한 속도와 위치에는 영향이 없으며,
                // 단지 렌더링이나 물리 계산을 처리할지 여부를 표시할 뿐이다.)
                firework->type = 0;

                // Add the payload
                // 페이로드(화약)을 추가한다.
                for (unsigned i = 0; i < rule->payloadCount; i++)
                {
                    FireworkRule::Payload * payload = rule->payloads + i;
                    create(payload->type, payload->count, firework);
                }
            }
        }
    }

    Application::update();
}

void FireworksDemo::display()
{
    const static cyclone::real size = 0.1f;

    // Clear the viewport and set the camera direction
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0.0 + CameraPos.x, 4.0 + CameraPos.y, 10.0 + CameraPos.z,  0.0 + CameraPos.x, 4.0 + CameraPos.y, 0.0 + CameraPos.z,  0.0, 1.0, 0.0);

    // Render each firework in turn
    glBegin(GL_QUADS);
    for (Firework *firework = fireworks;
        firework < fireworks+maxFireworks;
        firework++)
    {
        // Check if we need to process this firework.
        if (firework->type > 0)
        {
            switch (firework->type)
            {
            case 1: glColor3f(1,0,0); break;
            case 2: glColor3f(1,0.5f,0); break;
            case 3: glColor3f(1,1,0); break;
            case 4: glColor3f(0,1,0); break;
            case 5: glColor3f(0,1,1); break;
            case 6: glColor3f(0.4f,0.4f,1); break;
            case 7: glColor3f(1,0,1); break;
            case 8: glColor3f(1,1,1); break;
            case 9: glColor3f(1,0.5f,0.5f); break;
            };

            const cyclone::Vector3 &pos = firework->getPosition();
            glVertex3f(pos.x-size, pos.y-size, pos.z);
            glVertex3f(pos.x+size, pos.y-size, pos.z);
            glVertex3f(pos.x+size, pos.y+size, pos.z);
            glVertex3f(pos.x-size, pos.y+size, pos.z);

            // Render the firework's reflection
            glVertex3f(pos.x-size, -pos.y-size, pos.z);
            glVertex3f(pos.x+size, -pos.y-size, pos.z);
            glVertex3f(pos.x+size, -pos.y+size, pos.z);
            glVertex3f(pos.x-size, -pos.y+size, pos.z);
        }
    }
    glEnd();
}

void FireworksDemo::key(unsigned char key)
{
    switch (key)
    {
    case '1': create(1, 1, NULL); break;
    case '2': create(2, 1, NULL); break;
    case '3': create(3, 1, NULL); break;
    case '4': create(4, 1, NULL); break;
    case '5': create(5, 1, NULL); break;
    case '6': create(6, 1, NULL); break;
    case '7': create(7, 1, NULL); break;
    case '8': create(8, 1, NULL); break;
    case '9': create(10, 3, NULL); break;


    case 'w': 
        CameraPos.z -= 0.1;
        break;
    case 'a': 
        CameraPos.x -= 0.1;
        break;
    case 's': 
        CameraPos.z += 0.1;
        break;
    case 'd': 
        CameraPos.x += 0.1;
        break;

    }
}

void FireworksDemo::specialkey(unsigned char key)
{
    switch (key)
    {
    case GLUT_KEY_LEFT:  
        break;
    case GLUT_KEY_RIGHT:
        break;
    case GLUT_KEY_UP:
        break;
    case GLUT_KEY_DOWN:
        break;
    }
}

/**
 * Called by the common demo framework to create an application
 * object (with new) and return a pointer.
 */
Application* getApplication()
{
    return new FireworksDemo();
}
