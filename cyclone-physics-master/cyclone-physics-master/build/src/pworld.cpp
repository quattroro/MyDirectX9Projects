/*
 * Implementation file for random number generation.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cstddef>
#include <cyclone/pworld.h>

using namespace cyclone;

ParticleWorld::ParticleWorld(unsigned maxContacts, unsigned iterations)
:
resolver(iterations),
maxContacts(maxContacts)
{
    contacts = new ParticleContact[maxContacts];
    calculateIterations = (iterations == 0);

}

ParticleWorld::~ParticleWorld()
{
    delete[] contacts;
}

void ParticleWorld::startFrame()
{
    for (Particles::iterator p = particles.begin();
        p != particles.end();
        p++)
    {
        // Remove all forces from the accumulator
        (*p)->clearAccumulator();
    }
}

unsigned ParticleWorld::generateContacts()
{
    unsigned limit = maxContacts;
    ParticleContact *nextContact = contacts;

    for (ContactGenerators::iterator g = contactGenerators.begin();
        g != contactGenerators.end();
        g++)
    {
        //현재 존재하는 모든 플랫폼들과 모든 파티들들 각각 충돌했는지 확인하고 충돌했으면
        // contexts 배열에 값을 추가해준다.
        // 리턴값은 추가된 충돌처리의 개수
        unsigned used =(*g)->addContact(nextContact, limit);
        limit -= used;
        nextContact += used;

        // We've run out of contacts to fill. This means we're missing
        // contacts.
        if (limit <= 0) break;
    }

    // Return the number of contacts used.
    return maxContacts - limit;
}

void ParticleWorld::integrate(real duration)
{
    for (Particles::iterator p = particles.begin();
        p != particles.end();
        p++)
    {
        // Remove all forces from the accumulator
        (*p)->integrate(duration);
    }
}

// 여기서 물리를 업데이트 한다.
void ParticleWorld::runPhysics(real duration)
{
    // First apply the force generators
    // 각각의 입자들에 등록해준 힘 발생기를 이용해서 힘을 가해준다.
    registry.updateForces(duration);

    // Then integrate the objects
    // 적분을 수행
    integrate(duration);

    // Generate contacts
    //현재 존재하는 모든 플랫폼들과 모든 파티들들 각각 충돌했는지 확인하고 충돌했으면
    // contexts 배열에 값을 추가해준다.
    // 리턴값은 추가된 충돌처리의 개수
    unsigned usedContacts = generateContacts();

    // And process them
    if (usedContacts)
    {
        if (calculateIterations) resolver.setIterations(usedContacts * 2); // 그냥 임의의값으로 2배수 해준것
        resolver.resolveContacts(contacts, usedContacts, duration);
    }
}

ParticleWorld::Particles& ParticleWorld::getParticles()
{
    return particles;
}

ParticleWorld::ContactGenerators& ParticleWorld::getContactGenerators()
{
    return contactGenerators;
}

ParticleForceRegistry& ParticleWorld::getForceRegistry()
{
    return registry;
}

void GroundContacts::init(cyclone::ParticleWorld::Particles *particles)
{
    GroundContacts::particles = particles;
}

unsigned GroundContacts::addContact(cyclone::ParticleContact *contact,
                                    unsigned limit) const
{
    unsigned count = 0;
    for (cyclone::ParticleWorld::Particles::iterator p = particles->begin();
        p != particles->end();
        p++)
    {
        cyclone::real y = (*p)->getPosition().y;
        if (y < 0.0f)
        {
            contact->contactNormal = cyclone::Vector3::UP;
            contact->particle[0] = *p;
            contact->particle[1] = NULL;
            contact->penetration = -y;
            contact->restitution = 0.2f;
            contact++;
            count++;
        }

        if (count >= limit) return count;
    }
    return count;
}