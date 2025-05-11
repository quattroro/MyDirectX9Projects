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
     * 드래그 힘을 적용하는 힘 발생기
     * 스턴스 하나로 여러개의 입자에 두루 사용
     */
    class ParticleDrag : public ParticleForceGenerator
    {
        /** Holds the velocity drag coeffificent. */
        // 속도에 곱해지는 드래그 비례상수 저장
        real k1;

        /** Holds the velocity squared drag coeffificent. */
        // 속도의 제곱에 곱해지는 드래그 비례상수 저장
        real k2;

    public:

        /** Creates the generator with the given coefficients. */
        // 주어진 비례상수로 힘 발생기 개체 생성
        ParticleDrag(real k1, real k2);

        /** Applies the drag force to the given particle. */
        // 주어진 입자에 드래그 힘 적용
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a Spring force, where
     * one end is attached to a fixed point in space.
     * 한쪽 끝니 고정점에 연결된 스프링의 힘을 적용하는 힘 발생기
     */
    class ParticleAnchoredSpring : public ParticleForceGenerator
    {
    protected:
        /** The location of the anchored end of the spring. */
        // 스프링의 고정단 위치
        Vector3 *anchor;

        /** Holds the sprint constant. */
        // 스프링 상수
        real springConstant;

        /** Holds the rest length of the spring. */
        // 스프링 의 휴지 길이
        real restLength;

    public:
        ParticleAnchoredSpring();

        /** Creates a new spring with the given parameters. */
        // 주어진 인자를 바탕으로 새로운 스프링 인스턴스를 생성하는 생성자.
        ParticleAnchoredSpring(Vector3 *anchor,
                               real springConstant,
                               real restLength);

        /** Retrieve the anchor point. */
        // 
        const Vector3* getAnchor() const { return anchor; }

        /** Set the spring's properties. */
        void init(Vector3 *anchor,
                  real springConstant,
                  real restLength);

        /** Applies the spring force to the given particle. */
        // 주어진 입자에 스프링 힘을 적용한다.
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
    * A force generator that applies a bungee force, where
    * one end is attached to a fixed point in space.
    * 
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
     * 스프링 힘을 적용하는 힘 발생기
     */
    class ParticleSpring : public ParticleForceGenerator
    {
        /** The particle at the other end of the spring. */
        // 스프링의 반대쪽 끝의 입자.
        Particle *other;

        /** Holds the sprint constant. */
        // 스프링 상수
        real springConstant;

        /** Holds the rest length of the spring. */
        // 스프링의 휴지 길이
        real restLength;

    public:

        /** Creates a new spring with the given parameters. */
        // 주어진 인자들을 토대로 새로운 스프링 개체를 생성하는 생성자
        ParticleSpring(Particle *other,
            real springConstant, real restLength);

        /** Applies the spring force to the given particle. */
        // 주어진 입자에 힘을 적용한다.
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a spring force only
     * when extended.
     * 잡아 늘였을 때에만 스프링 힘을 적용하는 힘 발생기
     */
    class ParticleBungee : public ParticleForceGenerator
    {
        /** The particle at the other end of the spring. */
        // 스프링의 반대편에 끝에 있는 입다.
        Particle *other;

        /** Holds the sprint constant. */
        // 스프링 상수
        real springConstant;

        /**
         * Holds the length of the bungee at the point it begins to
         * generator a force.
         * 힘을 발생하기 시작하는 시점에서의 고무줄의 길이
         */
        real restLength;

    public:

        /** Creates a new bungee with the given parameters. */
        // 주어진 인자를 바탕으로 새로운 고무줄을 생성한다.
        ParticleBungee(Particle *other,
            real springConstant, real restLength);

        /** Applies the spring force to the given particle. */
        // 주어진 입자에 스프링 힘을 적용한다.
        virtual void updateForce(Particle *particle, real duration);
    };

    /**
     * A force generator that applies a buoyancy force for a plane of
     * liquid parrallel to XZ plane.
     * xz 편면에 평행한 수면에 대해 부력을 적용하는 힘 발생기
     * 
     */
    class ParticleBuoyancy : public ParticleForceGenerator
    {
        /**
         * The maximum submersion depth of the object before
         * it generates its maximum boyancy force.
         * 최대 부력을 발생하기 전 개체의 최대 침수 깊이
         */
        real maxDepth;

        /**
         * The volume of the object.
         * 물체의 부피
         */
        real volume;

        /**
         * The height of the water plane above y=0. The plane will be
         * parrallel to the XZ plane.
         * 수면이 y = 0 평면으로부터 이동한 높이
         * 수면은 xz 평면에 평행
         */
        real waterHeight;

        /**
         * The density of the liquid. Pure water has a density of
         * 1000kg per cubic meter.
         * 액체의 밀도 순수한 물의 밀도는 1000kg 이다.
         */
        real liquidDensity;

    public:

        /** Creates a new buoyancy force with the given parameters. */
        // 주어진 인자를 바탕으로 새로운 부력 개체 생성
        ParticleBuoyancy(real maxDepth, real volume, real waterHeight,
            real liquidDensity = 1000.0f);

        /** Applies the buoyancy force to the given particle. */
        // 주어진 입자에 부력 적용
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