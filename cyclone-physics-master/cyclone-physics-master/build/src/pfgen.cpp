/*
 * Implementation file for the particle force generators.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cyclone/pfgen.h>

using namespace cyclone;


void ParticleForceRegistry::updateForces(real duration)
{
    Registry::iterator i = registrations.begin();
    for (; i != registrations.end(); i++)
    {
        i->fg->updateForce(i->particle, duration);
    }
}

void ParticleForceRegistry::add(Particle* particle, ParticleForceGenerator *fg)
{
    ParticleForceRegistry::ParticleForceRegistration registration;
    registration.particle = particle;
    registration.fg = fg;
    registrations.push_back(registration);
}

ParticleGravity::ParticleGravity(const Vector3& gravity)
: gravity(gravity)
{
}

void ParticleGravity::updateForce(Particle* particle, real duration)
{
    // Check that we do not have infinite mass
    // 무한대 질량을 갖지 않는지 검사
    if (!particle->hasFiniteMass()) return;

    // Apply the mass-scaled force to the particle
    // 입자에 질량 크기에 따른 힘을 적용
    particle->addForce(gravity * particle->getMass());
}

ParticleDrag::ParticleDrag(real k1, real k2)
: k1(k1), k2(k2)
{
}

void ParticleDrag::updateForce(Particle* particle, real duration)
{
    Vector3 force;
    particle->getVelocity(&force);

    // Calculate the total drag coefficient
    // 토탈 드래그 계수를 게산한다.
    real dragCoeff = force.magnitude();
    dragCoeff = k1 * dragCoeff + k2 * dragCoeff * dragCoeff;

    // Calculate the final force and apply it
    // 최종 힘을 계산하여 적용시킨다.
    force.normalise();
    force *= -dragCoeff;
    particle->addForce(force);
}

ParticleSpring::ParticleSpring(Particle *other, real sc, real rl)
: other(other), springConstant(sc), restLength(rl)
{
}

void ParticleSpring::updateForce(Particle* particle, real duration)
{
    // Calculate the vector of the spring
    // 스프링 벡터 계산
    Vector3 force;
    particle->getPosition(&force);
    force -= other->getPosition();

    // Calculate the magnitude of the force
    // 힘의 크기 계산
    real magnitude = force.magnitude();
    magnitude = real_abs(magnitude - restLength);
    magnitude *= springConstant;

    // Calculate the final force and apply it
    // 최종 힘을 계산하여 입자에 적용
    force.normalise();
    force *= -magnitude;
    particle->addForce(force);
}

ParticleBuoyancy::ParticleBuoyancy(real maxDepth,
                                 real volume,
                                 real waterHeight,
                                 real liquidDensity)
:
maxDepth(maxDepth), volume(volume),
waterHeight(waterHeight), liquidDensity(liquidDensity)
{
}

void ParticleBuoyancy::updateForce(Particle* particle, real duration)
{
    // Calculate the submersion depth
    // 물속에 잠긴 깊이를 계산한다.
    real depth = particle->getPosition().y;

    // Check if we're out of the water
    // 물속인지 밖인지 검사한다.
    if (depth >= waterHeight + maxDepth) return;
    Vector3 force(0,0,0);

    // Check if we're at maximum depth
    // 최대 깊이인지 확인
    if (depth <= waterHeight - maxDepth)
    {
        force.y = liquidDensity * volume;
        particle->addForce(force);
        return;
    }

    // Otherwise we are partly submerged
    // 아니면, 부분적으로 잠겨 있음
    // 물 안에 완전히 잠기면 1, 물 밖으로 나오면 0이 된다.
    force.y = liquidDensity * volume *
        (depth - maxDepth - waterHeight) / (2 * maxDepth);
    particle->addForce(force);
}

ParticleBungee::ParticleBungee(Particle *other, real sc, real rl)
: other(other), springConstant(sc), restLength(rl)
{
}

void ParticleBungee::updateForce(Particle* particle, real duration)
{
    // Calculate the vector of the spring
    // 스프링 벡터 계산
    Vector3 force;
    particle->getPosition(&force);
    force -= other->getPosition();

    // Check if the bungee is compressed
    // 고무줄이 압축되었는지 검사
    real magnitude = force.magnitude();
    if (magnitude <= restLength) return;

    // Calculate the magnitude of the force
    // 힘의 크기 계산
    magnitude = springConstant * (restLength - magnitude);

    // Calculate the final force and apply it
    // 최종 힘을 계산하고 적용
    force.normalise();
    force *= -magnitude;
    particle->addForce(force);
}

ParticleFakeSpring::ParticleFakeSpring(Vector3 *anchor, real sc, real d)
: anchor(anchor), springConstant(sc), damping(d)
{
}

void ParticleFakeSpring::updateForce(Particle* particle, real duration)
{
    // Check that we do not have infinite mass
    if (!particle->hasFiniteMass()) return;

    // Calculate the relative position of the particle to the anchor
    Vector3 position;
    particle->getPosition(&position);
    position -= *anchor;

    // Calculate the constants and check they are in bounds.
    real gamma = 0.5f * real_sqrt(4 * springConstant - damping*damping);
    if (gamma == 0.0f) return;
    Vector3 c = position * (damping / (2.0f * gamma)) +
        particle->getVelocity() * (1.0f / gamma);

    // Calculate the target position
    Vector3 target = position * real_cos(gamma * duration) +
        c * real_sin(gamma * duration);
    target *= real_exp(-0.5f * duration * damping);

    // Calculate the resulting acceleration and therefore the force
    Vector3 accel = (target - position) * ((real)1.0 / (duration*duration)) -
        particle->getVelocity() * ((real)1.0/duration);
    particle->addForce(accel * particle->getMass());
}

ParticleAnchoredSpring::ParticleAnchoredSpring()
{
}

ParticleAnchoredSpring::ParticleAnchoredSpring(Vector3 *anchor,
                                               real sc, real rl)
: anchor(anchor), springConstant(sc), restLength(rl)
{
}

void ParticleAnchoredSpring::init(Vector3 *anchor, real springConstant,
                                  real restLength)
{
    ParticleAnchoredSpring::anchor = anchor;
    ParticleAnchoredSpring::springConstant = springConstant;
    ParticleAnchoredSpring::restLength = restLength;
}

void ParticleAnchoredBungee::updateForce(Particle* particle, real duration)
{
    // Calculate the vector of the spring
    Vector3 force;
    particle->getPosition(&force);
    force -= *anchor;

    // Calculate the magnitude of the force
    real magnitude = force.magnitude();
    if (magnitude < restLength) return;

    magnitude = magnitude - restLength;
    magnitude *= springConstant;

    // Calculate the final force and apply it
    force.normalise();
    force *= -magnitude;
    particle->addForce(force);
}

void ParticleAnchoredSpring::updateForce(Particle* particle, real duration)
{
    // Calculate the vector of the spring
    // 스프링 의 벡터 계산
    Vector3 force;
    particle->getPosition(&force);
    force -= *anchor;

    // Calculate the magnitude of the force
    // 힘의 크기 계산
    real magnitude = force.magnitude();
    magnitude = (restLength - magnitude) * springConstant;

    // Calculate the final force and apply it
    // 최종 힘을 계산하여 적용
    force.normalise();
    force *= magnitude;
    particle->addForce(force);
}
