/*
 * Interface file for the force generators.
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
 * This file contains the interface and sample force generators.
 */
#ifndef CYCLONE_PFGEN_H
#define CYCLONE_PFGEN_H

#include "core.h"
#include "particle.h"
#include <vector>

namespace cyclone {

    /**
     * A force generator can be asked to add a force to one or more
     * particles.
     */
    class ParticleForceGenerator
    {
    public:

        /**
         * Overload this in implementations of the interface to calculate
         * and update the force applied to the given particle.
         */
        virtual void updateForce(Particle *particle, real duration) = 0;
    };

    /**
     * A force generator that applies a gravitational force. One instance
     * can be used for multiple particles.
     * 중력을 적용하는 힘 발생기
     * 인스턴스 하나로 여러 개의 입자에 두루 사용할 수 있다.
     */
    class ParticleGravity : public ParticleForceGenerator
    {
        /** Holds the acceleration due to gravity. */
        // 중력에 의한 가속도 저장
        Vector3 gravity;

    public:

        /** Creates the generator with the given acceleration. */
        // 주어진 가속도로 힘 발생기 개체를 생성
        ParticleGravity(const Vector3 &gravity);

        /** Applies the gravitational force to the given particle. */
        // 주어진 입자에 중력 적용
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a drag force. One instance
     * can be used for multiple particles.
     */
    class ParticleDrag : public ParticleForceGenerator
    {
        /** Holds the velocity drag coeffificent. */
        real k1;

        /** Holds the velocity squared drag coeffificent. */
        real k2;

    public:

        /** Creates the generator with the given coefficients. */
        ParticleDrag(real k1, real k2);

        /** Applies the drag force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a Spring force, where
     * one end is attached to a fixed point in space.
     */
    class ParticleAnchoredSpring : public ParticleForceGenerator
    {
    protected:
        /** The location of the anchored end of the spring. */
        Vector3 *anchor;

        /** Holds the sprint constant. */
        real springConstant;

        /** Holds the rest length of the spring. */
        real restLength;

    public:
        ParticleAnchoredSpring();

        /** Creates a new spring with the given parameters. */
        ParticleAnchoredSpring(Vector3 *anchor,
                               real springConstant,
                               real restLength);

        /** Retrieve the anchor point. */
        const Vector3* getAnchor() const { return anchor; }

        /** Set the spring's properties. */
        void init(Vector3 *anchor,
                  real springConstant,
                  real restLength);

        /** Applies the spring force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
    * A force generator that applies a bungee force, where
    * one end is attached to a fixed point in space.
    */
    class ParticleAnchoredBungee : public ParticleAnchoredSpring
    {
    public:
        /** Applies the spring force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that fakes a stiff spring force, and where
     * one end is attached to a fixed point in space.
     */
    class ParticleFakeSpring : public ParticleForceGenerator
    {
        /** The location of the anchored end of the spring. */
        Vector3 *anchor;

        /** Holds the sprint constant. */
        real springConstant;

        /** Holds the damping on the oscillation of the spring. */
        real damping;

    public:

        /** Creates a new spring with the given parameters. */
        ParticleFakeSpring(Vector3 *anchor, real springConstant,
            real damping);

        /** Applies the spring force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a Spring force.
     */
    class ParticleSpring : public ParticleForceGenerator
    {
        /** The particle at the other end of the spring. */
        Particle *other;

        /** Holds the sprint constant. */
        real springConstant;

        /** Holds the rest length of the spring. */
        real restLength;

    public:

        /** Creates a new spring with the given parameters. */
        ParticleSpring(Particle *other,
            real springConstant, real restLength);

        /** Applies the spring force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a spring force only
     * when extended.
     */
    class ParticleBungee : public ParticleForceGenerator
    {
        /** The particle at the other end of the spring. */
        Particle *other;

        /** Holds the sprint constant. */
        real springConstant;

        /**
         * Holds the length of the bungee at the point it begins to
         * generator a force.
         */
        real restLength;

    public:

        /** Creates a new bungee with the given parameters. */
        ParticleBungee(Particle *other,
            real springConstant, real restLength);

        /** Applies the spring force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a buoyancy force for a plane of
     * liquid parrallel to XZ plane.
     */
    class ParticleBuoyancy : public ParticleForceGenerator
    {
        /**
         * The maximum submersion depth of the object before
         * it generates its maximum boyancy force.
         */
        real maxDepth;

        /**
         * The volume of the object.
         */
        real volume;

        /**
         * The height of the water plane above y=0. The plane will be
         * parrallel to the XZ plane.
         */
        real waterHeight;

        /**
         * The density of the liquid. Pure water has a density of
         * 1000kg per cubic meter.
         */
        real liquidDensity;

    public:

        /** Creates a new buoyancy force with the given parameters. */
        ParticleBuoyancy(real maxDepth, real volume, real waterHeight,
            real liquidDensity = 1000.0f);

        /** Applies the buoyancy force to the given particle. */
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * Holds all the force generators and the particles they apply to.
     * 힘 발생기와 적용하는 대상 입자의 모든 목록을 기록해둔다.
     */
    class ParticleForceRegistry
    {
    protected:

        /**
         * Keeps track of one force generator and the particle it
         * applies to.
         * 하나의 힘 발생기와 힘을 적용할 입자를 추적
         */
        struct ParticleForceRegistration
        {
            Particle *particle;
            ParticleForceGenerator *fg;
        };

        /**
         * Holds the list of registrations.
         * registrations라는 목록으로 유지
         */
        typedef std::vector<ParticleForceRegistration> Registry;
        Registry registrations;

    public:
        /**
         * Registers the given force generator to apply to the
         * given particle.
         * 주어진 힘 발생기를 주어진 입자에 등록
         */
        void add(Particle* particle, ParticleForceGenerator *fg);

        /**
         * Removes the given registered pair from the registry.
         * If the pair is not registered, this method will have
         * no effect.
         * 주어진 등록된 쌍을 목록에서 제거한다.
         * 해당 쌍이 등록되지 않았다면, 
         * 이 메서드는 아무런 영향도 끼치지 않는다.
         */
        void remove(Particle* particle, ParticleForceGenerator *fg);

        /**
         * Clears all registrations from the registry. This will
         * not delete the particles or the force generators
         * themselves, just the records of their connection.
         * 모든 등록 항목을 제거한다.
         * 입자나 힘 발생기 자체는 제거하지 않고,
         * 그 연결성만 제거한다.
         */
        void clear();

        /**
         * Calls all the force generators to update the forces of
         * their corresponding particles.
         * 해당하는 입자의 힘을 업데이트하도록 모든 힘 발생기 호출
         */
        void updateForces(real duration);
    };
}

#endif // CYCLONE_PFGEN_H