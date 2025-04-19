/*
 * Implementation file for the particle class.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <assert.h>
#include <cyclone/particle.h>

using namespace cyclone;


/*
 * --------------------------------------------------------------------------
 * 입자에 대하여, 주어진 시간 간격만큼 적분을 수행한다.
 * 이 함수는 뉴튼-오일러 적분법을 사용하는데, 적분오차를 보정하기 위해 선형근사(linear approximation)
 * 를 사용한다. 그래서 때로는 부정확할 수 있다.
 * --------------------------------------------------------------------------
 */

void Particle::integrate(real duration)
{
    // 무한대 질량인 입자는 적분을 하지 않는다.
    if (inverseMass <= 0.0f) return;

    assert(duration > 0.0);

    // 위치를 업데이트 한다.
    position.addScaledVector(velocity, duration);

    // 힘으로부터 가속도를 계산한다.
    // 힘이 여러 종류가 있으면 이 벡터에 가속도를 더해준다.
    Vector3 resultingAcc = acceleration;
    resultingAcc.addScaledVector(forceAccum, inverseMass);

    // 가속도로부터 속도를 업데이트한다.
    velocity.addScaledVector(resultingAcc, duration);

    // 드래그를 적용한다.
    velocity *= real_pow(damping, duration);

    // 힘 항목을 지운다.
    clearAccumulator();
}



void Particle::setMass(const real mass)
{
    assert(mass != 0);
    Particle::inverseMass = ((real)1.0)/mass;
}

real Particle::getMass() const
{
    if (inverseMass == 0) {
        return REAL_MAX;
    } else {
        return ((real)1.0)/inverseMass;
    }
}

void Particle::setInverseMass(const real inverseMass)
{
    Particle::inverseMass = inverseMass;
}

real Particle::getInverseMass() const
{
    return inverseMass;
}

bool Particle::hasFiniteMass() const
{
    return inverseMass >= 0.0f;
}

void Particle::setDamping(const real damping)
{
    Particle::damping = damping;
}

real Particle::getDamping() const
{
    return damping;
}

void Particle::setPosition(const Vector3 &position)
{
    Particle::position = position;
}

void Particle::setPosition(const real x, const real y, const real z)
{
    position.x = x;
    position.y = y;
    position.z = z;
}

void Particle::getPosition(Vector3 *position) const
{
    *position = Particle::position;
}

Vector3 Particle::getPosition() const
{
    return position;
}

void Particle::setVelocity(const Vector3 &velocity)
{
    Particle::velocity = velocity;
}

void Particle::setVelocity(const real x, const real y, const real z)
{
    velocity.x = x;
    velocity.y = y;
    velocity.z = z;
}

void Particle::getVelocity(Vector3 *velocity) const
{
    *velocity = Particle::velocity;
}

Vector3 Particle::getVelocity() const
{
    return velocity;
}

void Particle::setAcceleration(const Vector3 &acceleration)
{
    Particle::acceleration = acceleration;
}

void Particle::setAcceleration(const real x, const real y, const real z)
{
    acceleration.x = x;
    acceleration.y = y;
    acceleration.z = z;
}

void Particle::getAcceleration(Vector3 *acceleration) const
{
    *acceleration = Particle::acceleration;
}

Vector3 Particle::getAcceleration() const
{
    return acceleration;
}

void Particle::clearAccumulator()
{
    forceAccum.clear();
}

void Particle::addForce(const Vector3 &force)
{
    forceAccum += force;
}
