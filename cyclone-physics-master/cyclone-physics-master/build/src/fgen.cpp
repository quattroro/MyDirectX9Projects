/*
 * Implementation file for the rigid body force generators.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under license. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software license.
 */

#include <cyclone/fgen.h>

using namespace cyclone;

void ForceRegistry::updateForces(real duration)
{
    Registry::iterator i = registrations.begin();
    for (; i != registrations.end(); i++)
    {
        i->fg->updateForce(i->body, duration);
    }
}

void ForceRegistry::add(RigidBody *body, ForceGenerator *fg)
{
    ForceRegistry::ForceRegistration registration;
    registration.body = body;
    registration.fg = fg;
    registrations.push_back(registration);
}

Buoyancy::Buoyancy(const Vector3 &cOfB, real maxDepth, real volume,
                   real waterHeight, real liquidDensity /* = 1000.0f */)
{
    centreOfBuoyancy = cOfB;
    Buoyancy::liquidDensity = liquidDensity;
    Buoyancy::maxDepth = maxDepth;
    Buoyancy::volume = volume;
    Buoyancy::waterHeight = waterHeight;
}

void Buoyancy::updateForce(RigidBody *body, real duration)
{
    // Calculate the submersion depth
    Vector3 pointInWorld = body->getPointInWorldSpace(centreOfBuoyancy);
    real depth = pointInWorld.y;

    // Check if we're out of the water
    if (depth >= waterHeight + maxDepth) return;
    Vector3 force(0,0,0);

    // Check if we're at maximum depth
    if (depth <= waterHeight - maxDepth)
    {
        force.y = liquidDensity * volume;
        body->addForceAtBodyPoint(force, centreOfBuoyancy);
        return;
    }

    // Otherwise we are partly submerged
    force.y = liquidDensity * volume *
        (depth - maxDepth - waterHeight) / 2 * maxDepth;
    body->addForceAtBodyPoint(force, centreOfBuoyancy);
}

Gravity::Gravity(const Vector3& gravity)
: gravity(gravity)
{
}

void Gravity::updateForce(RigidBody* body, real duration)
{
    // Check that we do not have infinite mass
    // 질량이 무한대인지 검사한다.
    if (!body->hasFiniteMass()) return;

    // Apply the mass-scaled force to the body
    // 중력 가속도에 질량을 곱한 만큼의 힘을 적용시킨다.
    body->addForce(gravity * body->getMass());
}

Spring::Spring(const Vector3 &localConnectionPt,
               RigidBody *other,
               const Vector3 &otherConnectionPt,
               real springConstant,
               real restLength)
: connectionPoint(localConnectionPt),
  otherConnectionPoint(otherConnectionPt),
  other(other),
  springConstant(springConstant),
  restLength(restLength)
{
}

void Spring::updateForce(RigidBody* body, real duration)
{
    // Calculate the two ends in world space
    // 양쪽 끝점을 월드 좌표계로 계산한다.
    Vector3 lws = body->getPointInWorldSpace(connectionPoint);
    Vector3 ows = other->getPointInWorldSpace(otherConnectionPoint);

    // Calculate the vector of the spring
    // 스프링 벡터를 계산한다.
    Vector3 force = lws - ows;

    // Calculate the magnitude of the force
    // 힘의 크기를 계산한다.
    real magnitude = force.magnitude();
    magnitude = real_abs(magnitude - restLength);
    magnitude *= springConstant;

    // Calculate the final force and apply it
    // 최종 힘을 계산하여 적용한다.
    force.normalise();
    force *= -magnitude;
    body->addForceAtPoint(force, lws);
}

Aero::Aero(const Matrix3 &tensor, const Vector3 &position, const Vector3 *windspeed)
{
    Aero::tensor = tensor;
    Aero::position = position;
    Aero::windspeed = windspeed;
}

void Aero::updateForce(RigidBody *body, real duration)
{
    Aero::updateForceFromTensor(body, duration, tensor);
}

void Aero::updateForceFromTensor(RigidBody *body, real duration,
                                 const Matrix3 &tensor)
{
    // Calculate total velocity (windspeed and body's velocity).
    // 속도를 계산한다.(공기의 속도와 물체의 속도)
    Vector3 velocity = body->getVelocity();
    velocity += *windspeed;

    // Calculate the velocity in body coordinates
    // 속도 벡터를 물체의 로컬 좌표로 변환한다.
    Vector3 bodyVel = body->getTransform().transformInverseDirection(velocity);

    // Calculate the force in body coordinates
    // 힘을 물체의 로컬 좌표로 변환한다.
    Vector3 bodyForce = tensor.transform(bodyVel);
    Vector3 force = body->getTransform().transformDirection(bodyForce);

    // Apply the force
    // 힘을 적용한다.
    body->addForceAtBodyPoint(force, position);
}

AeroControl::AeroControl(const Matrix3 &base, const Matrix3 &min, const Matrix3 &max,
                              const Vector3 &position, const Vector3 *windspeed)
:
Aero(base, position, windspeed)
{
    AeroControl::minTensor = min;
    AeroControl::maxTensor = max;
    controlSetting = 0.0f;
}

Matrix3 AeroControl::getTensor()
{
    if (controlSetting <= -1.0f) return minTensor;
    else if (controlSetting >= 1.0f) return maxTensor;
    else if (controlSetting < 0)
    {
        return Matrix3::linearInterpolate(minTensor, tensor, controlSetting+1.0f);
    }
    else if (controlSetting > 0)
    {
        return Matrix3::linearInterpolate(tensor, maxTensor, controlSetting);
    }
    else return tensor;
}

void AeroControl::setControl(real value)
{
    controlSetting = value;
}

void AeroControl::updateForce(RigidBody *body, real duration)
{
    Matrix3 tensor = getTensor();
    Aero::updateForceFromTensor(body, duration, tensor);
}

void Explosion::updateForce(RigidBody* body, real duration)
{

}