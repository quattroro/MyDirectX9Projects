/*
 * Interface file for the particle / mass aggregate world structure.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

/**
 * @file
 *
 * This file contains the definitions for a structure to hold any number o
 * particle masses, and their connections.
 */
#ifndef CYCLONE_PWORLD_H
#define CYCLONE_PWORLD_H

#include "pfgen.h"
#include "plinks.h"

namespace cyclone {

    /**
     * Keeps track of a set of particles, and provides the means to
     * update them all.
     */
    class ParticleWorld
    {
    public:
        typedef std::vector<Particle*> Particles;
        typedef std::vector<ParticleContactGenerator*> ContactGenerators;

    protected:
        /**
         * Holds the particles
         * 입자들을 저장한다.
         */
        Particles particles;

        /**
         * True if the world should calculate the number of iterations
         * to give the contact resolver at each frame.
         */
        bool calculateIterations;

        /**
         * Holds the force generators for the particles in this world.
         * world에 속한 입자의 에 대한 힘 발생기를 등록해준다.
         */
        ParticleForceRegistry registry;

        /**
         * Holds the resolver for contacts.
         * 접촉 처리기
         */
        ParticleContactResolver resolver;

        /**
         * Contact generators.
         * 접촉 발생기
         */
        ContactGenerators contactGenerators;

        /**
         * Holds the list of contacts.
         * 접촉들의 목록
         */
        ParticleContact *contacts;

        /**
         * Holds the maximum number of contacts allowed (i.e. the
         * size of the contacts array).
         * 접촉의 최대 개수, 즉, 접촉 배열의 크기
         */
        unsigned maxContacts;

    public:

        /**
         * Creates a new particle simulator that can handle up to the
         * given number of contacts per frame. You can also optionally
         * give a number of contact-resolution iterations to use. If you
         * don't give a number of iterations, then twice the number of
         * contacts will be used.
         * 주어진 입자들을 처리하는 새로운 입자 시뮬레이터 개체를 생성하는 생성자
         * 반복적 접촉 처리기를 지정할 수도 있다.
         * 반복 횟수를 별도로 지정하지 않으면,
         * 접촉접 개수의 두 배로 지정된다.
         */
        ParticleWorld(unsigned maxContacts, unsigned iterations=0);

        /**
         * Deletes the simulator.
         */
        ~ParticleWorld();

        /**
         * Calls each of the registered contact generators to report
         * their contacts. Returns the number of generated contacts.
         * 등록된 접촉 발생기를 호출하여 접촉 유무를 보고하게 한다.
         * 반환값은 생성된 접촉의 개수이다.
         */
        unsigned generateContacts();

        /**
         * Integrates all the particles in this world forward in time
         * by the given duration.
         * 주어진 시강 간격 동안 world에 속한 모든 입자들에 대하여 접분을 수행한다.
         */
        void integrate(real duration);

        /**
         * Processes all the physics for the particle world.
         * world에 대하여 모든 물리 연산 처리
         */
        void runPhysics(real duration);

        /**
         * Initializes the world for a simulation frame. This clears
         * the force accumulators for particles in the world. After
         * calling this, the particles can have their forces for this
         * frame added.
         * 시뮬레이션 프레임을 위해 world를 초기화 한다.
         * 이 함수에서는 모든 입자들에 대하여 누적된 힘을 지운다.
         * 이 함수를 호출한 다음에는 이번 프레임 동안 작용한 힘이 더해진다.
         */
        void startFrame();

        /**
         *  Returns the list of particles.
         */
        Particles& getParticles();

        /**
         * Returns the list of contact generators.
         */
        ContactGenerators& getContactGenerators();

        /**
         * Returns the force registry.
         */
        ParticleForceRegistry& getForceRegistry();
    };

    /**
      * A contact generator that takes an STL vector of particle pointers and
     * collides them against the ground.
     */
    class GroundContacts : public cyclone::ParticleContactGenerator
    {
        cyclone::ParticleWorld::Particles *particles;

    public:
        void init(cyclone::ParticleWorld::Particles *particles);

        virtual unsigned addContact(cyclone::ParticleContact *contact,
            unsigned limit) const;
    };

} // namespace cyclone

#endif // CYCLONE_PWORLD_H
