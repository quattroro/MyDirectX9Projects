/*
 * Interface file for the rigid body world structure.
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
 * This file contains the definitions for a structure to hold any
 * number of rigid bodies, and to manage their simulation.
 */
#ifndef CYCLONE_WORLD_H
#define CYCLONE_WORLD_H

#include "body.h"
#include "contacts.h"

namespace cyclone {
    /**
     * The world represents an independent simulation of physics.  It
     * keeps track of a set of rigid bodies, and provides the means to
     * update them all.
     * 
     * world는 물리 시뮬레이션을 표현한다.
     * 시뮬레이션 대상 물체들을 관리하고, 이들을 업데이트할 수단을 제공한다.
     */
    class World
    {
        // ... other World data as before ...
        /**
         * True if the world should calculate the number of iterations
         * to give the contact resolver at each frame.
         */
        bool calculateIterations;

        /**
         * Holds a single rigid body in a linked list of bodies.
         */
        struct BodyRegistration
        {
            RigidBody *body;
            BodyRegistration * next;
        };

        /**
         * Holds the head of the list of registered bodies.
         */
        BodyRegistration *firstBody;

        /**
         * Holds the resolver for sets of contacts.
         */
        ContactResolver resolver;

        /**
         * Holds one contact generators in a linked list.
         */
        struct ContactGenRegistration
        {
            ContactGenerator *gen;
            ContactGenRegistration *next;
        };

        /**
         * Holds the head of the list of contact generators.
         */
        ContactGenRegistration *firstContactGen;

        /**
         * Holds an array of contacts, for filling by the contact
         * generators.
         */
        Contact *contacts;

        /**
         * Holds the maximum number of contacts allowed (i.e. the size
         * of the contacts array).
         */
        unsigned maxContacts;

    public:
        /**
         * Creates a new simulator that can handle up to the given
         * number of contacts per frame. You can also optionally give
         * a number of contact-resolution iterations to use. If you
         * don't give a number of iterations, then four times the
         * number of detected contacts will be used for each frame.
         */
        World(unsigned maxContacts, unsigned iterations=0);
        ~World();

        /**
         * Calls each of the registered contact generators to report
         * their contacts. Returns the number of generated contacts.
         */
        unsigned generateContacts();

        /**
         * Processes all the physics for the world.
         * 
         * world의 모든 물리 연산을 처리한다.
         */
        void runPhysics(real duration);

        /**
         * Initialises the world for a simulation frame. This clears
         * the force and torque accumulators for bodies in the
         * world. After calling this, the bodies can have their forces
         * and torques for this frame added.
         * 
         * 시뮬레이션 프레임이 시작될 때마다 World를 초기화한다.
         * 이 메서드는 각 물체에 대한 힘 누적기와 토크 누적기를 초기화한다.
         * 이 함수를 호출한 다음 현재 프레임에 대하여 각 물체를 적용하는 힘과
         * 토크를 계산한다.
         */
        void startFrame();

    };

} // namespace cyclone

#endif // CYCLONE_PWORLD_H
