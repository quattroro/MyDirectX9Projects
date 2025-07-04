/*
 * Interface file for the force generators.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under license. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software license.
 */

/**
 * @file
 *
 * This file contains the interface and sample force generators.
 */
#ifndef CYCLONE_FGEN_H
#define CYCLONE_FGEN_H

#include "body.h"
#include "pfgen.h"
#include <vector>

namespace cyclone {

    /**
     * A force generator can be asked to add a force to one or more
     * bodies.
     * 
     * 힘 발생기는 물체에 힘을 가하는 역할을 한다.
     */
    class ForceGenerator
    {
    public:

        /**
         * Overload this in implementations of the interface to calculate
         * and update the force applied to the given rigid body.
         * 
         * 이 함수를 재정의하여 주어진 강체에 가해지는 힘을 계산하고
         * 업데이트하는 인터페이스를 구현한다.
         */
        virtual void updateForce(RigidBody *body, real duration) = 0;
    };

    /**
     * A force generator that applies a gravitational force. One instance
     * can be used for multiple rigid bodies.
     * 
     * 중력을 적용하는 힘 발생기
     * 인스턴스 하나로 여러 개의 물체에 두루 사용 가능하다.
     */
    class Gravity : public ForceGenerator
    {
        /** Holds the acceleration due to gravity. */
        // 중력에 의한 가속도
        Vector3 gravity;

    public:

        /** Creates the generator with the given acceleration. */
        // 주어진 가속도로 힘 발생기 개체를 생성하는 생성자.
        Gravity(const Vector3 &gravity);

        /** Applies the gravitational force to the given rigid body. */
        // 주어진 강체에 중력을 적용한다.
        virtual void updateForce(RigidBody *body, real duration);
    };

    /**
     * A force generator that applies a Spring force.
     * 스프링 힘을 적용하는 힘 발생기
     */
    class Spring : public ForceGenerator
    {
        /**
         * The point of connection of the spring, in local
         * coordinates.
         * 스프링이 붙어 있는 위치를 로컬 좌표계로 저장해둔다.
         */
        Vector3 connectionPoint;

        /**
         * The point of connection of the spring to the other object,
         * in that object's local coordinates.
         * 스프링이 반대쪽에 붙어 있는 물체에 대하여
         * 붙어 있는 위치를 해당 물체의 로컬 좌표계로 저장해둔다.
         */
        Vector3 otherConnectionPoint;

        /** The particle at the other end of the spring. */
        // 스프링의 반대쪽에 분어 있는 물체
        RigidBody *other;

        /** Holds the sprint constant. */
        // 스프링 상수
        real springConstant;

        /** Holds the rest length of the spring. */
        // 스프링의 휴지 길이
        real restLength;

    public:

        /** Creates a new spring with the given parameters. */
        // 주어진 인자들로부터 스프링 힘 발생기 개체를 만든다.
        Spring(const Vector3 &localConnectionPt,
               RigidBody *other,
               const Vector3 &otherConnectionPt,
               real springConstant,
               real restLength);

        /** Applies the spring force to the given rigid body. */
        // 주어진 강체에 스프링 힘을 적용한다.
        virtual void updateForce(RigidBody *body, real duration);
    };

    /**
     * A force generator showing a three component explosion effect.
     * This force generator is intended to represent a single
     * explosion effect for multiple rigid bodies. The force generator
     * can also act as a particle force generator.
     */
    class Explosion : public ForceGenerator,
                      public ParticleForceGenerator
    {
        /**
         * Tracks how long the explosion has been in operation, used
         * for time-sensitive effects.
         */
        real timePassed;

    public:
        // Properties of the explosion, these are public because
        // there are so many and providing a suitable constructor
        // would be cumbersome:

        /**
         * The location of the detonation of the weapon.
         */
        Vector3 detonation;

        // ... Other Explosion code as before ...


        /**
         * The radius up to which objects implode in the first stage
         * of the explosion.
         */
        real implosionMaxRadius;

        /**
         * The radius within which objects don't feel the implosion
         * force. Objects near to the detonation aren't sucked in by
         * the air implosion.
         */
        real implosionMinRadius;

        /**
         * The length of time that objects spend imploding before the
         * concussion phase kicks in.
         */
        real implosionDuration;

        /**
         * The maximal force that the implosion can apply. This should
         * be relatively small to avoid the implosion pulling objects
         * through the detonation point and out the other side before
         * the concussion wave kicks in.
         */
        real implosionForce;

        /**
         * The speed that the shock wave is traveling, this is related
         * to the thickness below in the relationship:
         *
         * thickness >= speed * minimum frame duration
         */
        real shockwaveSpeed;

        /**
         * The shock wave applies its force over a range of distances,
         * this controls how thick. Faster waves require larger
         * thicknesses.
         */
        real shockwaveThickness;

        /**
         * This is the force that is applied at the very centre of the
         * concussion wave on an object that is stationary. Objects
         * that are in front or behind of the wavefront, or that are
         * already moving outwards, get proportionally less
         * force. Objects moving in towards the centre get
         * proportionally more force.
         */
         real peakConcussionForce;

         /**
          * The length of time that the concussion wave is active.
          * As the wave nears this, the forces it applies reduces.
          */
         real concussionDuration;

         /**
          * This is the peak force for stationary objects in
          * the centre of the convection chimney. Force calculations
          * for this value are the same as for peakConcussionForce.
          */
         real peakConvectionForce;

         /**
          * The radius of the chimney cylinder in the xz plane.
          */
         real chimneyRadius;

         /**
          * The maximum height of the chimney.
          */
         real chimneyHeight;

         /**
          * The length of time the convection chimney is active. Typically
          * this is the longest effect to be in operation, as the heat
          * from the explosion outlives the shock wave and implosion
          * itself.
          */
         real convectionDuration;

    public:
        /**
         * Creates a new explosion with sensible default values.
         */
        Explosion();

        /**
         * Calculates and applies the force that the explosion
         * has on the given rigid body.
         */
        virtual void updateForce(RigidBody * body, real duration);

        /**
         * Calculates and applies the force that the explosion has
         * on the given particle.
         */
        virtual void updateForce(Particle *particle, real duration) = 0;

    };

    /**
     * A force generator that applies an aerodynamic force.
     * 공기역학적 힘을 적용하는 힘 발생기
     */
    class Aero : public ForceGenerator
    {
    protected:
        /**
         * Holds the aerodynamic tensor for the surface in body
         * space.
         * 물체의 로컬 좌표를 기준으로 공기역학 텐서를 저장해준다.
         */
        Matrix3 tensor;

        /**
         * Holds the relative position of the aerodynamic surface in
         * body coordinates.
         * 물체의 로컬 좌표를 기준으로
         * 힘이 작용할 공기역학적 표면의 상대 위치를 저장한다.
         */
        Vector3 position;

        /**
         * Holds a pointer to a vector containing the windspeed of the
         * environment. This is easier than managing a separate
         * windspeed vector per generator and having to update it
         * manually as the wind changes.
         * 외부 환경에서의 공기의 속도 벡터를 가리키는 포인터
         * 포인터 형태로 두면 힘 발생기가 여러 개 있을 경우
         * 바람이 바뀌었을 때 공기의 속도 벡터를 
         * 일일이 업데이트해 주지 않아도 되므로 편리하다.
         */
        const Vector3* windspeed;

    public:
        /**
         * Creates a new aerodynamic force generator with the
         * given properties.
         * 주어진 속성으로 새로운 공기역학 힘 발생기 개체를 만드는 생성자.
         */
        Aero(const Matrix3 &tensor, const Vector3 &position,
             const Vector3 *windspeed);

        /**
         * Applies the force to the given rigid body.
         * 주어진 강체에 힘을 작용시킨다.
         */
        virtual void updateForce(RigidBody *body, real duration);

    protected:
        /**
         * Uses an explicit tensor matrix to update the force on
         * the given rigid body. This is exactly the same as for updateForce
         * only it takes an explicit tensor.
         * 주어진 강체에 주어진 텐서를 이용해 힘을 적용시킨다.
         * 텐서가 주어진다는 점만 빼면 updateForce 함수와 완전히 같다.
         */
        void updateForceFromTensor(RigidBody *body, real duration,
                                   const Matrix3 &tensor);
    };

    /**
    * A force generator with a control aerodynamic surface. This
    * requires three inertia tensors, for the two extremes and
    * 'resting' position of the control surface.  The latter tensor is
    * the one inherited from the base class, the two extremes are
    * defined in this class.
    * 공기역학적 표면에 대한 힘 발생기
    * 여기에는 관성 텐서 세개가 필요한데,
    * 두 개는 조종면이 양쪽 끝까지 움직였을 때의 것이고,
    * 하나는 조종면이 중립(resting)위치일 떄의 것이다.
    * 중립 위치에서의 텐서는 부모 클래스의 것을 사용하고
    * 조종면이 끝까지 움직였을 때의 두 텐서는 여기서 정의한다.
    */
    class AeroControl : public Aero
    {
    protected:
        /**
         * The aerodynamic tensor for the surface, when the control is at
         * its maximum value.
         * 조종면이 최대 위치로 움직였을 때의 공기역학 텐서
         */
        Matrix3 maxTensor;

        /**
         * The aerodynamic tensor for the surface, when the control is at
         * its minimum value.
         * 조종면이 최소 위치로 움직였을 때의 공기역학 텐서
         */
        Matrix3 minTensor;

        /**
        * The current position of the control for this surface. This
        * should range between -1 (in which case the minTensor value
        * is used), through 0 (where the base-class tensor value is
        * used) to +1 (where the maxTensor value is used).
        * 조종면의 현재 위치.
        * 이 값은 -1(minTensor가 사용됨)에서
        * 0(부모 클래스의 텐서가 사용됨)을 거쳐
        * +1(maxTensor가 사용됨)까지 변할 수 있다.
        */
        real controlSetting;

    private:
        /**
         * Calculates the final aerodynamic tensor for the current
         * control setting.
         * 조종면의 현재 위치에 따른 공기역학 텐서를 계산한다.
         */
        Matrix3 getTensor();

    public:
        /**
         * Creates a new aerodynamic control surface with the given
         * properties.
         * 주어진 속성을 바탕으로 새로운 조종면 개체를 만드는 생성자.
         */
        AeroControl(const Matrix3 &base,
                    const Matrix3 &min, const Matrix3 &max,
                    const Vector3 &position, const Vector3 *windspeed);

        /**
         * Sets the control position of this control. This * should
        range between -1 (in which case the minTensor value is *
        used), through 0 (where the base-class tensor value is used) *
        to +1 (where the maxTensor value is used). Values outside that
        * range give undefined results.
        * 조종면의 위치를 설정한다.
        * 이 값은 -1(minTensor가 사용됨)에서
        * 0(부모 클래스의 텐서가 사용됨)을 거쳐
        * +1(maxTensor가 사용됨)까지 값을 가질 수 있다.
        * 이 범위를 벗어나는 값을 사용했을 때의 결과는 정의되지 않는다.
        */
        void setControl(real value);

        /**
         * Applies the force to the given rigid body.
         * 주어진 강체에 힘을 적용한다.
         */
        virtual void updateForce(RigidBody *body, real duration);
    };

    /**
     * A force generator with an aerodynamic surface that can be
     * re-oriented relative to its rigid body. This derives the
     */
    class AngledAero : public Aero
    {
        /**
         * Holds the orientation of the aerodynamic surface relative
         * to the rigid body to which it is attached.
         */
        Quaternion orientation;

    public:
        /**
         * Creates a new aerodynamic surface with the given properties.
         */
        AngledAero(const Matrix3 &tensor, const Vector3 &position,
             const Vector3 *windspeed);

        /**
         * Sets the relative orientation of the aerodynamic surface,
         * relative to the rigid body it is attached to. Note that
         * this doesn't affect the point of connection of the surface
         * to the body.
         */
        void setOrientation(const Quaternion &quat);

        /**
         * Applies the force to the given rigid body.
         */
        virtual void updateForce(RigidBody *body, real duration);
    };

    /**
     * A force generator to apply a buoyant force to a rigid body.
     */
    class Buoyancy : public ForceGenerator
    {
        /**
         * The maximum submersion depth of the object before
         * it generates its maximum buoyancy force.
         */
        real maxDepth;

        /**
         * The volume of the object.
         */
        real volume;

        /**
         * The height of the water plane above y=0. The plane will be
         * parallel to the XZ plane.
         */
        real waterHeight;

        /**
         * The density of the liquid. Pure water has a density of
         * 1000kg per cubic meter.
         */
        real liquidDensity;

        /**
         * The centre of buoyancy of the rigid body, in body coordinates.
         */
        Vector3 centreOfBuoyancy;

    public:

        /** Creates a new buoyancy force with the given parameters. */
        Buoyancy(const Vector3 &cOfB,
            real maxDepth, real volume, real waterHeight,
            real liquidDensity = 1000.0f);

        /**
         * Applies the force to the given rigid body.
         */
        virtual void updateForce(RigidBody *body, real duration);
    };

    /**
    * Holds all the force generators and the bodies they apply to.
    */
    class ForceRegistry
    {
    protected:

        /**
        * Keeps track of one force generator and the body it
        * applies to.
        */
        struct ForceRegistration
        {
            RigidBody *body;
            ForceGenerator *fg;
        };

        /**
        * Holds the list of registrations.
        */
        typedef std::vector<ForceRegistration> Registry;
        Registry registrations;

    public:
        /**
        * Registers the given force generator to apply to the
        * given body.
        */
        void add(RigidBody* body, ForceGenerator *fg);

        /**
        * Removes the given registered pair from the registry.
        * If the pair is not registered, this method will have
        * no effect.
        */
        void remove(RigidBody* body, ForceGenerator *fg);

        /**
        * Clears all registrations from the registry. This will
        * not delete the bodies or the force generators
        * themselves, just the records of their connection.
        */
        void clear();

        /**
        * Calls all the force generators to update the forces of
        * their corresponding bodies.
        */
        void updateForces(real duration);
    };
}


#endif // CYCLONE_FGEN_H