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
         * ���ڵ��� �����Ѵ�.
         */
        Particles particles;

        /**
         * True if the world should calculate the number of iterations
         * to give the contact resolver at each frame.
         */
        bool calculateIterations;

        /**
         * Holds the force generators for the particles in this world.
         * world�� ���� ������ �� ���� �� �߻��⸦ ������ش�.
         */
        ParticleForceRegistry registry;

        /**
         * Holds the resolver for contacts.
         * ���� ó����
         */
        ParticleContactResolver resolver;

        /**
         * Contact generators.
         * ���� �߻���
         */
        ContactGenerators contactGenerators;

        /**
         * Holds the list of contacts.
         * ���˵��� ���
         */
        ParticleContact *contacts;

        /**
         * Holds the maximum number of contacts allowed (i.e. the
         * size of the contacts array).
         * ������ �ִ� ����, ��, ���� �迭�� ũ��
         */
        unsigned maxContacts;

    public:

        /**
         * Creates a new particle simulator that can handle up to the
         * given number of contacts per frame. You can also optionally
         * give a number of contact-resolution iterations to use. If you
         * don't give a number of iterations, then twice the number of
         * contacts will be used.
         * �־��� ���ڵ��� ó���ϴ� ���ο� ���� �ùķ����� ��ü�� �����ϴ� ������
         * �ݺ��� ���� ó���⸦ ������ ���� �ִ�.
         * �ݺ� Ƚ���� ������ �������� ������,
         * ������ ������ �� ��� �����ȴ�.
         */
        ParticleWorld(unsigned maxContacts, unsigned iterations=0);

        /**
         * Deletes the simulator.
         */
        ~ParticleWorld();

        /**
         * Calls each of the registered contact generators to report
         * their contacts. Returns the number of generated contacts.
         * ��ϵ� ���� �߻��⸦ ȣ���Ͽ� ���� ������ �����ϰ� �Ѵ�.
         * ��ȯ���� ������ ������ �����̴�.
         */
        unsigned generateContacts();

        /**
         * Integrates all the particles in this world forward in time
         * by the given duration.
         * �־��� �ð� ���� ���� world�� ���� ��� ���ڵ鿡 ���Ͽ� ������ �����Ѵ�.
         */
        void integrate(real duration);

        /**
         * Processes all the physics for the particle world.
         * world�� ���Ͽ� ��� ���� ���� ó��
         */
        void runPhysics(real duration);

        /**
         * Initializes the world for a simulation frame. This clears
         * the force accumulators for particles in the world. After
         * calling this, the particles can have their forces for this
         * frame added.
         * �ùķ��̼� �������� ���� world�� �ʱ�ȭ �Ѵ�.
         * �� �Լ������� ��� ���ڵ鿡 ���Ͽ� ������ ���� �����.
         * �� �Լ��� ȣ���� �������� �̹� ������ ���� �ۿ��� ���� ��������.
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
