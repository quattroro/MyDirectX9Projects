/*
 * Implementation file for the particle contact resolution system.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cyclone/pcontacts.h>

using namespace cyclone;

// Contact implementation

void ParticleContact::resolve(real duration)
{
    resolveVelocity(duration);
    resolveInterpenetration(duration);
}

// 접촉 방향에 대한 속도를 계산한다.
// 접촉 방향으로 움직여야하는지 아니면 반대로 움직여야하는지는 어디서 구분하지?...
// contactNormal을 계산할때 움직여야하는 방향으로 세팅이 되어지는거 같다.
real ParticleContact::calculateSeparatingVelocity() const
{
    Vector3 relativeVelocity = particle[0]->getVelocity();
    if (particle[1]) relativeVelocity -= particle[1]->getVelocity();
    return relativeVelocity * contactNormal;
}

void ParticleContact::resolveVelocity(real duration)
{
    // Find the velocity in the direction of the contact
    // 접촉 방향에 대한 속도를 계산한다.
    real separatingVelocity = calculateSeparatingVelocity();

    // Check if it needs to be resolved
    // 접촉을 처리해야 하는지 검사
    if (separatingVelocity > 0)
    {
        // The contact is either separating, or stationary - there's
        // no impulse required.
        // 접촉이 일어났으나 물체가 분리되고 있거나,
        // 가만히 있을 경우 충격량이 필요 없다.
        return;
    }

    // Calculate the new separating velocity
    // 새롭게 계산된 분리 속도.
    real newSepVelocity = -separatingVelocity * restitution;

    ///////////////////////////////////////////////////////////////////////////////////////////// 정지 접촉 처리 시작
    // Check the velocity build-up due to acceleration only
    // 속도가 가속도만에 의한 것인지를 검사한다.
    Vector3 accCausedVelocity = particle[0]->getAcceleration();
    if (particle[1]) accCausedVelocity -= particle[1]->getAcceleration();
    real accCausedSepVelocity = accCausedVelocity * contactNormal * duration;

    // If we've got a closing velocity due to acceleration build-up,
    // remove it from the new separating velocity
    // 가속도에 의해 속도가 생겼으면,
    // 이를 새로운 분리 속도에서 제거한다.
    if (accCausedSepVelocity < 0)
    {
        newSepVelocity += restitution * accCausedSepVelocity;

        // Make sure we haven't removed more than was
        // there to remove.
        // 실제 필요한 것보다 더 많이 빼내지는 않았는지 확인한다.
        if (newSepVelocity < 0) newSepVelocity = 0;
    }
    ///////////////////////////////////////////////////////////////////////////////////////////// 정지 접촉 처리 끝

    real deltaVelocity = newSepVelocity - separatingVelocity;

    // We apply the change in velocity to each object in proportion to
    // their inverse mass (i.e. those with lower inverse mass [higher
    // actual mass] get less change in velocity)..
    // 각각의 물체에 대하여 질량에 반비례하여 속도를 변경한다.
    // 즉, 질량이 작은 물체일수록 속도의 변화가 심하다.
    real totalInverseMass = particle[0]->getInverseMass();
    if (particle[1]) totalInverseMass += particle[1]->getInverseMass();

    // If all particles have infinite mass, then impulses have no effect
    // 모든 물체의 질량이 무한대이면 충격량의 효과가 없다.
    if (totalInverseMass <= 0) return;

    // Calculate the impulse to apply
    // 적용할 충격량을 계산한다.
    real impulse = deltaVelocity / totalInverseMass;

    // Find the amount of impulse per unit of inverse mass
    // 반비례하는 질량 당위당 충격량을 계산한다.
    Vector3 impulsePerIMass = contactNormal * impulse;

    // Apply impulses: they are applied in the direction of the contact,
    // and are proportional to the inverse mass.
    // 충격량을 적용한다. 충격량을 접촉 방향으로,
    // 질량에 반비례하여 작용한다.
    particle[0]->setVelocity(particle[0]->getVelocity() +
        impulsePerIMass * particle[0]->getInverseMass()
        );
    if (particle[1])
    {
        // Particle 1 goes in the opposite direction
        // 입자 1에는 반대 방향으로 작용한다.
        particle[1]->setVelocity(particle[1]->getVelocity() +
            impulsePerIMass * -particle[1]->getInverseMass()
            );
    }
}

// 물체의 겹쳐짐 처리
void ParticleContact::resolveInterpenetration(real duration)
{
    // If we don't have any penetration, skip this step.
    // 겹쳐진 부분이 없으면 이 단계는 건너뛴다.
    if (penetration <= 0) return;

    // The movement of each object is based on their inverse mass, so
    // total that.
    // 물체를 옮겨주는 거리는 질량에 반리례하므로 질량을 다 더한다.
    real totalInverseMass = particle[0]->getInverseMass();
    if (particle[1]) totalInverseMass += particle[1]->getInverseMass();

    // If all particles have infinite mass, then we do nothing
    // 모든 물체의 잘량리 무한대이면 아무런 처리도 하지 않는다.
    if (totalInverseMass <= 0) return;

    // Find the amount of penetration resolution per unit of inverse mass
    // 물체가 옮겨갈 거리는 질량에 반비례한다.
    Vector3 movePerIMass = contactNormal * (penetration / totalInverseMass);

    // Calculate the the movement amounts
    // 물체가 옮겨갈 거리는 질량에 반비례한다.
    // 1번 물체에 -를 곱해준 이유는 접점의 법선벡터는 0번 물체를 기준으로 한 것이기 때문에 반대 방향으로 옮겨가야 하기 때문이다.
    particleMovement[0] = movePerIMass * particle[0]->getInverseMass();
    if (particle[1]) {
        particleMovement[1] = movePerIMass * -particle[1]->getInverseMass();
    } else {
        particleMovement[1].clear();
    }

    // Apply the penetration resolution
    // 물체를 옮긴다.
    particle[0]->setPosition(particle[0]->getPosition() + particleMovement[0]);
    if (particle[1]) {
        particle[1]->setPosition(particle[1]->getPosition() + particleMovement[1]);
    }
}

ParticleContactResolver::ParticleContactResolver(unsigned iterations)
:
iterations(iterations)
{
}

void ParticleContactResolver::setIterations(unsigned iterations)
{
    ParticleContactResolver::iterations = iterations;
}

// 세팅된 접촉을 처리한다.
void ParticleContactResolver::resolveContacts(ParticleContact *contactArray,
                                              unsigned numContacts,
                                              real duration)
{
    unsigned i;

    iterationsUsed = 0;
    while(iterationsUsed < iterations)
    {
        // Find the contact with the largest closing velocity;
        // 접근 속도가 가장 큰 접촉을 찾는다.
        real max = REAL_MAX;
        unsigned maxIndex = numContacts;
        for (i = 0; i < numContacts; i++)
        {
            real sepVel = contactArray[i].calculateSeparatingVelocity();
            if (sepVel < max &&
                (sepVel < 0 || contactArray[i].penetration > 0))
            {
                max = sepVel;
                maxIndex = i;
            }
        }

        // Do we have anything worth resolving?
        // 오류 검사
        if (maxIndex == numContacts) break;

        // Resolve this contact
        // 이 접촉을 처리한다.
        contactArray[maxIndex].resolve(duration);

        // Update the interpenetrations for all particles
        Vector3 *move = contactArray[maxIndex].particleMovement;
        for (i = 0; i < numContacts; i++)
        {
            if (contactArray[i].particle[0] == contactArray[maxIndex].particle[0])
            {
                contactArray[i].penetration -= move[0] * contactArray[i].contactNormal;
            }
            else if (contactArray[i].particle[0] == contactArray[maxIndex].particle[1])
            {
                contactArray[i].penetration -= move[1] * contactArray[i].contactNormal;
            }
            if (contactArray[i].particle[1])
            {
                if (contactArray[i].particle[1] == contactArray[maxIndex].particle[0])
                {
                    contactArray[i].penetration += move[0] * contactArray[i].contactNormal;
                }
                else if (contactArray[i].particle[1] == contactArray[maxIndex].particle[1])
                {
                    contactArray[i].penetration += move[1] * contactArray[i].contactNormal;
                }
            }
        }

        iterationsUsed++;
    }
}
