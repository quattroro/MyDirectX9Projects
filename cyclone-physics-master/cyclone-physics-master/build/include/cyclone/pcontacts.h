/*
 * Interface file for the contact resolution system for particles.
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
 * This file contains the contact resolution system for particles
 * cyclone.
 *
 * The resolver uses an iterative satisfaction algorithm; it loops
 * through each contact and tries to resolve it. This is a very fast
 * algorithm but can be unstable when the contacts are highly
 * inter-related.
 
 */
#ifndef CYCLONE_PCONTACTS_H
#define CYCLONE_PCONTACTS_H

#include "particle.h"

namespace cyclone {

    /*
     * Forward declaration, see full declaration below for complete
     * documentation.
     */
    class ParticleContactResolver;

    /**
     * A Contact represents two objects in contact (in this case
     * ParticleContact representing two Particles). Resolving a
     * contact removes their interpenetration, and applies sufficient
     * impulse to keep them apart. Colliding bodies may also rebound.
     *
     * The contact has no callable functions, it just holds the
     * contact details. To resolve a set of contacts, use the particle
     * contact resolver class.
     * * 두 물체의 접촉을 처리한다.
     * 접촉을 처리하기 위해서는 물체 간 간섭을 제거하고,
     * 적절한 반발력을 적용하여 물체를 떼어놓아야 한다.
     * 접촉하는 물체가 다시 뛸 때도 있다.
     * 
     * 접촉에는 호출할 수 있는 함수가 없고,
     * 접촉의 세부내용을 갖고 있을 뿐이다.
     * 접촉을 처리하려면 입자 접촉 해소 클래스를 사용한다.
     */
    class ParticleContact
    {
        // ... Other ParticleContact code as before ...


        /**
         * The contact resolver object needs access into the contacts to
         * set and effect the contact.
         */
        friend class ParticleContactResolver;


    public:
        /**
         * Holds the particles that are involved in the contact. The
         * second of these can be NULL, for contacts with the scenery.
         * 접촉에 포함되는 입자들을 저장한다.
         * 배경과 접촉하는 경우는 NULL이 될 수 있다.
         */
        Particle* particle[2];

        /**
         * Holds the normal restitution coefficient at the contact.
         * 접촉점에서의 반발 계수
         */
        real restitution;

        /**
         * Holds the direction of the contact in world coordinates.
         * 월드 좌표계 기준 접촉 방향 벡터
         */
        Vector3 contactNormal;

        /**
         * Holds the depth of penetration at the contact.
         * 접촉의 결과 얼마나 겹쳐져 있는지를 저장한다.
         * 이 값이 음수면 겹쳐진 부분이 없다는 뜻이고, 0이면 살짝 닿기만 했다는 뜻이다.
         */
        real penetration;

        /**
         * Holds the amount each particle is moved by during interpenetration
         * resolution.
         */
        Vector3 particleMovement[2];

    protected:
        /**
         * Resolves this contact, for both velocity and interpenetration.
         * 속도와 겹치는 부분을 업데이트하여 접촉을 해소한다.
         */
        void resolve(real duration);

        /**
         * Calculates the separating velocity at this contact.
         * 접촉으로 인한 분리 속도를 계산한다.
         */
        real calculateSeparatingVelocity() const;

    private:
        /**
         * Handles the impulse calculations for this collision.
         * 접촉에 대한 충격량 계한을 처리한다.
         */
        void resolveVelocity(real duration);

        /**
         * Handles the interpenetration resolution for this contact.
         * 접촉에 의해 겹치는 부분을 처리한다.
         */
        void resolveInterpenetration(real duration);

    };

    /**
     * The contact resolution routine for particle contacts. One
     * resolver instance can be shared for the whole simulation.
     * 입자 접촉에 대한 접촉 처리기
     * 인스턴스는 하나만 만들어서 돌려쓴다.
     */
    class ParticleContactResolver
    {
    protected:
        /**
         * Holds the number of iterations allowed.
         * 반복 횟수.
         */
        unsigned iterations;

        /**
         * This is a performance tracking value - we keep a record
         * of the actual number of iterations used.
         * 실제 동작한 반복 횟수를 기록한다.
         * 성능 측정이 목적.
         */
        unsigned iterationsUsed;

    public:
        /**
         * Creates a new contact resolver.
         * 새로운 접촉 처리기 개체를 생성한다.
         */
        ParticleContactResolver(unsigned iterations);

        /**
         * Sets the number of iterations that can be used.
         * 최대 반복 횟수를 지정한다.
         */
        void setIterations(unsigned iterations);

        /**
         * Resolves a set of particle contacts for both penetration
         * and velocity.
         *
         * Contacts that cannot interact with each other should be
         * passed to separate calls to resolveContacts, as the
         * resolution algorithm takes much longer for lots of contacts
         * than it does for the same number of contacts in small sets.
         *
         * @param contactArray Pointer to an array of particle contact
         * objects.
         *
         * @param numContacts The number of contacts in the array to
         * resolve.
         *
         * @param numIterations The number of iterations through the
         * resolution algorithm. This should be at least the number of
         * contacts (otherwise some constraints will not be resolved -
         * although sometimes this is not noticable). If the
         * iterations are not needed they will not be used, so adding
         * more iterations may not make any difference. But in some
         * cases you would need millions of iterations. Think about
         * the number of iterations as a bound: if you specify a large
         * number, sometimes the algorithm WILL use it, and you may
         * drop frames.
         *
         * @param duration The duration of the previous integration step.
         * This is used to compensate for forces applied.
        */
        // 겹쳐진 부분과 속도에 대해 입자들의 접촉을 처리한다.
        void resolveContacts(ParticleContact *contactArray,
            unsigned numContacts,
            real duration);
    };

    /**
     * This is the basic polymorphic interface for contact generators
     * applying to particles.
     */
    class ParticleContactGenerator
    {
    public:
        /**
         * Fills the given contact structure with the generated
         * contact. The contact pointer should point to the first
         * available contact in a contact array, where limit is the
         * maximum number of contacts in the array that can be written
         * to. The method returns the number of contacts that have
         * been written.
         */
        virtual unsigned addContact(ParticleContact *contact,
                                    unsigned limit) const = 0;
    };



} // namespace cyclone

#endif // CYCLONE_CONTACTS_H
