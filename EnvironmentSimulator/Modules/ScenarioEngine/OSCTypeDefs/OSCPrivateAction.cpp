/*
 * esmini - Environment Simulator Minimalistic
 * https://github.com/esmini/esmini
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) partners of Simulation Scenarios
 * https://sites.google.com/view/simulationscenarios
 */

#define _USE_MATH_DEFINES
#include <math.h>

#include "OSCPrivateAction.hpp"
#include "ScenarioEngine.hpp"

#define MAX_DECELERATION                -8.0
#define LONGITUDINAL_DISTANCE_THRESHOLD 0.1
#define LATERAL_DISTANCE_THRESHOLD      0.01

using namespace scenarioengine;

// Equations used in TransitionDynamics
// ------------------------------------
// a = start value
// b = target delta (target value = start value + b)
// c = parameter range (e.g. duration or distance)
// Example: Starting at lateral position 0.5 (a), move 1.0 (b) to 1.5 in 2 seconds (c)
//
// Step: Trivial
// Linear: Trivial
// Sinusoidal:
//		f(x) = a-b*(cos(pi*x/c)-1)/2
//		f'(x) = pi*b*sin(pi*x/c)/(2*c)
//		Slope peaks midway, at x=c/2 =>
//		f'peak = pi*b/(2*c)
//		f''(x): b*pi^2*cos(pi*x/c)/(2c^2)
//		Acceleration peaks at endpoints (x=0, x=c)
//      f''peak: f''(0) = pi^2*b/(2c^2), f''(c) = -b*pi^2/(2c^2)
//      plot: "https://www.desmos.com/calculator/ikrmaen0mm"
//		f': "https://www.wolframalpha.com/input/?i=d/dx(a-b*(cos(pi*x/c)-1)/2)"
//		f'peak: "https://www.wolframalpha.com/input/?i=d/dx(a-b*(cos(pi*x/c)-1)/2),x=c/2"
//		f'': "https://www.wolframalpha.com/input/?i=d/dx(pi*b*sin(pi*x/c)/(2*c))"
// Cubic:
//		f(x) = a+3b(x/c)^2-2b(x/c)^3
//		f'(x) = 6bx(c-x)/c^3
//		Slope (rate) peaks midway, at x=c/2 =>
//      f'peak = f'(c/2) = 3*b/(2*c)
//      f''(x) = 6b(c-2x)/c^3
//		Acceleration peaks at endpoints (x=0, x=c)
//      f''peak: f''(0) = 6b/c^2, f''(c) = -6b/c^2
//		plot: "https://www.desmos.com/calculator/6t6wbeeos8"
//		f': "https://www.wolframalpha.com/input/?i=d/dx(a%2B3b(x/c)^2-2b(x/c)^3)"
//		f'peak: "https://www.wolframalpha.com/input/?i=d/dx(a%2B3b(x/c)^2-2b(x/c)^3),x=c/2"
//      f'': Trivial

double OSCPrivateAction::TransitionDynamics::EvaluatePrimPeak()
{
    if (dimension_ == DynamicsDimension::RATE)
    {
        return GetRate();
    }
    else
    {
        if (shape_ == DynamicsShape::STEP || (dimension_ != DynamicsDimension::RATE && GetParamTargetVal() < SMALL_NUMBER))
        {
            return LARGE_NUMBER * SIGN(GetTargetVal() - GetStartVal());
        }
        else if (shape_ == DynamicsShape::LINEAR)
        {
            return (GetTargetVal() - GetStartVal()) / AVOID_ZERO(GetParamTargetVal());
        }
        else if (shape_ == DynamicsShape::SINUSOIDAL)
        {
            return M_PI * (GetTargetVal() - GetStartVal()) / (2 * AVOID_ZERO(GetParamTargetVal()));
        }
        else if (shape_ == DynamicsShape::CUBIC)
        {
            return 1.5 * (GetTargetVal() - GetStartVal()) / AVOID_ZERO(GetParamTargetVal());
        }
        else
        {
            LOG_ERROR("Invalid Dynamics shape: {}", shape_);
        }
    }

    return 0;
}

double OSCPrivateAction::TransitionDynamics::GetTargetParamValByPrimPeak(double prim_peak)
{
    if (shape_ == DynamicsShape::STEP)
    {
        return 0.0;
    }
    else if (shape_ == DynamicsShape::LINEAR)
    {
        return (GetTargetVal() - GetStartVal()) / prim_peak;
    }
    else if (shape_ == DynamicsShape::SINUSOIDAL)
    {
        return M_PI * (GetTargetVal() - GetStartVal()) / (2 * prim_peak);
    }
    else if (shape_ == DynamicsShape::CUBIC)
    {
        return 1.5 * (GetTargetVal() - GetStartVal()) / prim_peak;
    }
    else
    {
        LOG_ERROR("Invalid Dynamics shape: {}", shape_);
    }

    return 0.0;
}

double OSCPrivateAction::TransitionDynamics::GetTargetParamValByPrimPrimPeak(double prim_prim_peak)
{
    if (shape_ == DynamicsShape::STEP)
    {
        return 0.0;
    }
    else if (shape_ == DynamicsShape::LINEAR)
    {
        // Acceleration is infinite at start and end, anything else should result in flat line
        // Just to have something reasonable, re-use acc equation from CUBIC case
        return sqrt(6 * abs(GetTargetVal() - GetStartVal()) / prim_prim_peak);
    }
    else if (shape_ == DynamicsShape::SINUSOIDAL)
    {
        // pi*sqrt(abs(b)/(2*prim_prim_peak))
        return M_PI * sqrt(abs(GetTargetVal() - GetStartVal()) / (2 * prim_prim_peak));
    }
    else if (shape_ == DynamicsShape::CUBIC)
    {
        // sqrt(6*abs(b)/y)
        return sqrt(6 * abs(GetTargetVal() - GetStartVal()) / prim_prim_peak);
    }
    else
    {
        LOG_ERROR("Invalid Dynamics shape: {}", shape_);
    }

    return 0.0;
}

double OSCPrivateAction::TransitionDynamics::EvaluatePrim()
{
    if (shape_ == DynamicsShape::STEP || (dimension_ != DynamicsDimension::RATE && GetParamTargetVal() < SMALL_NUMBER))
    {
        return LARGE_NUMBER * SIGN(GetTargetVal() - GetStartVal());
    }
    else if (shape_ == DynamicsShape::LINEAR)
    {
        return (GetTargetVal() - GetStartVal()) / AVOID_ZERO(GetParamTargetVal());
    }
    else if (shape_ == DynamicsShape::SINUSOIDAL)
    {
        return M_PI * (GetTargetVal() - GetStartVal()) * sin(M_PI * param_val_ / GetParamTargetVal()) / (2 * AVOID_ZERO(GetParamTargetVal()));
    }
    else if (shape_ == DynamicsShape::CUBIC)
    {
        return 6 * (GetTargetVal() - GetStartVal()) * (GetParamTargetVal() - param_val_) * param_val_ / pow(AVOID_ZERO(GetParamTargetVal()), 3);
    }
    else
    {
        LOG_ERROR("Invalid Dynamics shape: {}", shape_);
    }

    return 0;
}

double OSCPrivateAction::TransitionDynamics::EvaluateScaledPrim()
{
    return EvaluatePrim() / AVOID_ZERO(scale_factor_);
}

double OSCPrivateAction::TransitionDynamics::Evaluate(DynamicsShape shape) const
{
    if (shape == DynamicsShape::SHAPE_UNDEFINED)
    {
        shape = shape_;
    }

    if (shape_ == DynamicsShape::STEP || (dimension_ != DynamicsDimension::RATE && GetParamTargetVal() < SMALL_NUMBER))
    {
        // consider empty parameter range (start = target = 0) as step function
        return GetTargetVal();
    }
    else if (shape == DynamicsShape::LINEAR)
    {
        return GetStartVal() + GetParamVal() * (GetTargetVal() - GetStartVal()) / (AVOID_ZERO(GetParamTargetVal()));
    }
    else if (shape == DynamicsShape::SINUSOIDAL)
    {
        return GetStartVal() - (GetTargetVal() - GetStartVal()) * (cos(M_PI * GetParamVal() / AVOID_ZERO(GetParamTargetVal())) - 1) / 2;
    }
    else if (shape == DynamicsShape::CUBIC)
    {
        return GetStartVal() + (GetTargetVal() - GetStartVal()) * pow(GetParamVal() / AVOID_ZERO(GetParamTargetVal()), 2) *
                                   (3 - 2 * GetParamVal() / AVOID_ZERO(GetParamTargetVal()));
    }
    else
    {
        LOG_ERROR("Invalid Dynamics shape: {}", shape);
    }

    return GetTargetVal();
}

void OSCPrivateAction::TransitionDynamics::Reset()
{
    scale_factor_ = 1.0;
    param_val_    = 0.0;
    start_val_    = 0.0;
    target_val_   = 0.0;
}

int OSCPrivateAction::TransitionDynamics::Step(double delta_param_val)
{
    param_val_ += delta_param_val / scale_factor_;

    if (param_val_ > param_target_val_)
    {
        param_val_ = param_target_val_;
    }
    else if (param_val_ < 0.0)
    {
        param_val_ = 0.0;
    }

    return 0;
}

void OSCPrivateAction::TransitionDynamics::SetStartVal(double start_val)
{
    start_val_ = start_val;
    UpdateRate();
}

void OSCPrivateAction::TransitionDynamics::SetTargetVal(double target_val)
{
    target_val_ = target_val;
    UpdateRate();
}

void OSCPrivateAction::TransitionDynamics::SetParamTargetVal(double target_value)
{
    if (dimension_ != DynamicsDimension::RATE)
    {
        param_target_val_ = target_value;
    }
    else
    {
        // Interpret the target parameter value as rate
        SetRate(target_value);
    }
}

void OSCPrivateAction::TransitionDynamics::SetMaxRate(double max_rate)
{
    // Check max rate
    double peak_rate = EvaluatePrimPeak();

    if (abs(peak_rate) > abs(max_rate))
    {
        scale_factor_ = abs(peak_rate) / abs(AVOID_ZERO(max_rate));
    }
    else
    {
        scale_factor_ = 1.0;
    }
}

void OSCPrivateAction::TransitionDynamics::SetRate(double rate)
{
    // Adapt sign
    rate_ = rate;

    UpdateRate();
}

void OSCPrivateAction::TransitionDynamics::UpdateRate()
{
    // Adapt sign
    rate_ = SIGN(GetTargetVal() - GetStartVal()) * abs(rate_);

    if (dimension_ == DynamicsDimension::RATE && !NEAR_ZERO(rate_))
    {
        // Find out parameter range from rate
        param_target_val_ = AVOID_ZERO(GetTargetParamValByPrimPeak(rate_));
    }
}

AssignRouteAction::~AssignRouteAction()
{
    object_->pos_.SetRoute(nullptr);
    if (route_ != nullptr)
    {
        delete route_;
        route_ = nullptr;
    }
}

void AssignRouteAction::Start(double simTime)
{
    route_->setObjName(object_->GetName());
    object_->pos_.SetRoute(route_);
    object_->SetDirtyBits(Object::DirtyBit::ROUTE);

    OSCAction::Start(simTime);
}

void AssignRouteAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

void AssignRouteAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }
    for (size_t i = 0; i < route_->minimal_waypoints_.size(); i++)
    {
        route_->minimal_waypoints_[i].ReplaceObjectRefs(&obj1->pos_, &obj2->pos_);
    }
}

void FollowTrajectoryAction::Start(double simTime)
{
    OSCAction::Start(simTime);

    // Check whether to ignore heading for trajectory motion direction. Prioritize option over object property.
    if (SE_Env::Inst().GetOptions().GetOptionSet("ignore_heading_for_traj_motion"))
    {
        SetIgnoreHeadingForMotion(true);
    }
    else if (object_ != nullptr && object_->properties_.ValueExists("ignoreHeadingForTrajMotion"))
    {
        SetIgnoreHeadingForMotion(object_->properties_.GetValueStr("ignoreHeadingForTrajMotion") == "true");
    }

    // global interpolation option highest prio, then per object property, then based on action followingMode
    if (SE_Env::Inst().GetOptions().GetOptionSet("pline_interpolation"))
    {
        std::string interp_mode = SE_Env::Inst().GetOptions().GetOptionArg("pline_interpolation");
        if (interp_mode == "off")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_NONE);
        }
        else if (interp_mode == "segment")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_SEGMENT);
        }
        else if (interp_mode == "corner")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_CORNER);
        }
        else
        {
            // fallback to no interpolation
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_NONE);
        }
    }
    else if (object_ != nullptr && object_->properties_.ValueExists("plineInterpolation"))
    {
        std::string interp_mode = object_->properties_.GetValueStr("plineInterpolation");
        if (interp_mode == "segment")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_SEGMENT);
        }
        else if (interp_mode == "corner")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_CORNER);
        }
        else if (interp_mode == "off")
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_NONE);
        }
    }
    else
    {
        if (following_mode_ == FollowingMode::FOLLOW)
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_SEGMENT);
        }
        else if (following_mode_ == FollowingMode::POSITION)
        {
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_CORNER);
        }
        else
        {
            // fallback to no interpolation
            traj_->shape_->pline_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_NONE);
        }
    }

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        // other action or controller already updated lateral dimension of object
        // potentially longitudinal dimension could be updated separatelly - but skip that for now
        return;
    }

    traj_->Freeze(following_mode_, object_->GetSpeed(), &object_->pos_);
    object_->pos_.SetTrajectory(traj_);

    // establish initial position along trajectory (s) and register initial time
    object_->pos_.SetTrajectoryS(initialDistanceOffset_);
    time_ = traj_->GetTime();

    // establish speed sign / driving direction, default is driving forward
    double speedSign = 1.0;
    if (timing_domain_ == TimingDomain::NONE)
    {
        speedSign = SIGN(object_->GetSpeed());
    }
    else
    {
        // for timestamp mode, speed sign is given by heading if set
        if (traj_->IsHSetExplicitly())
        {
            speedSign = GetAbsAngleDifference(traj_->GetH(), traj_->GetHTrue()) > M_PI_2 + SMALL_NUMBER ? -1 : 1;
        }
    }

    // establish initial heading
    if (traj_->IsHSetExplicitly())
    {
        object_->pos_.SetHeading(traj_->GetH());
        explicit_h_active_ = true;
    }
    else
    {
        // heading not specified: align with trajectory, forward or backwards according to speed sign
        object_->pos_.SetHeading(GetAngleInInterval2PI(traj_->GetHTrue() + (speedSign < 0 ? M_PI : 0.0)));
        explicit_h_active_ = false;
    }

    initialHeadingSign_ = GetAbsAngleDifference(object_->pos_.GetH(), traj_->GetHTrue()) > M_PI_2 + SMALL_NUMBER ? -1 : 1;

    if (traj_->IsHSetExplicitly() && ignore_heading_for_motion_)
    {
        // ignore heading for motion
        initialHeadingSign_ = 1;
    }

    // establish initial speed if time reference is defined
    if (timing_domain_ != TimingDomain::NONE)
    {
        object_->SetSpeed(speedSign * traj_->GetSpeed());
    }
}

FollowTrajectoryAction::~FollowTrajectoryAction()
{
    if (traj_)
    {
        delete traj_;
        traj_ = nullptr;
    }
}

void FollowTrajectoryAction::End()
{
    OSCAction::End();

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        return;
    }

    // remove trajectory from object, but double check the trajectory comes from this action
    if (object_->pos_.GetTrajectory() == traj_)
    {
        object_->pos_.SetTrajectory(nullptr);
    }
}

void FollowTrajectoryAction::Step(double simTime, double dt)
{
    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        // other action or controller already updated lateral dimension of object
        // potentially longitudinal dimension could be updated separatelly - but skip that for now
        return;
    }

    // signal that an action owns control
    object_->SetDirtyBits(Object::DirtyBit::LATERAL | Object::DirtyBit::LONGITUDINAL | Object::DirtyBit::SPEED);

    if (!object_->IsGhost() && simTime < 0.0)
    {
        // Only ghosts are moving up to and including time == 0
        return;
    }

    double old_s = object_->pos_.GetTrajectoryS();

    // Adjust absolute time for any ghost headstart
    double timeOffset = (timing_domain_ == TimingDomain::TIMING_ABSOLUTE && object_->IsGhost()) ? object_->GetHeadstartTime() : 0.0;

    Move(simTime, dt);

    // Check end conditions:
    // Trajectories with no time stamps:
    //     closed trajectories have no end
    //     open trajectories simply ends when s >= length of trajectory
    // Trajectories with time stamps:
    //     always ends when time >= trajectory duration (last timestamp)
    if (((timing_domain_ == TimingDomain::NONE && !traj_->closed_) && dt > 0.0 &&
         ((movingDirection_ * fabs(object_->GetSpeed()) > 0.0 && object_->pos_.GetTrajectoryS() > (traj_->GetLength() - SMALL_NUMBER)) ||
          (movingDirection_ * fabs(object_->GetSpeed()) < 0.0 && object_->pos_.GetTrajectoryS() < SMALL_NUMBER))) ||
        (timing_domain_ != TimingDomain::NONE && time_ + timeOffset >= traj_->GetStartTime() + traj_->GetDuration()))
    {
        // Reached end, or start, of trajectory
        double remaningDistance = 0.0;
        if (timing_domain_ == TimingDomain::NONE && !traj_->closed_ &&
            (object_->pos_.GetTrajectoryS() > (traj_->GetLength() - SMALL_NUMBER) || (object_->pos_.GetTrajectoryS() < SMALL_NUMBER)))
        {
            // Move the remaning distance along road at current lane offset
            remaningDistance = fabs(object_->speed_) * dt - fabs(object_->pos_.GetTrajectoryS() - old_s);
        }
        else if (timing_domain_ != TimingDomain::NONE && time_ + timeOffset >= traj_->GetStartTime() + traj_->GetDuration())
        {
            // Move the remaning distance along road at current lane offset
            double remaningTime = time_ + timeOffset - (traj_->GetStartTime() + traj_->GetDuration());
            remaningDistance    = remaningTime * object_->speed_;
        }

        // Move the remainder of distance along the current heading
        double dx = remaningDistance * cos(traj_->GetH());
        double dy = remaningDistance * sin(traj_->GetH());

        object_->pos_.SetInertiaPos(object_->pos_.GetX() + dx,
                                    object_->pos_.GetY() + dy,
                                    std::nan(""),   // don't update Z
                                    std::nan(""),   // don't update Heading
                                    std::nan(""),   // don't update Pitch
                                    std::nan(""));  // don't update Roll

        End();
    }
}

void scenarioengine::FollowTrajectoryAction::Move(double simTime, double dt)
{
    double old_s = object_->pos_.GetTrajectoryS();

    // Adjust absolute time for any ghost headstart
    double timeOffset = (timing_domain_ == TimingDomain::TIMING_ABSOLUTE && object_->IsGhost()) ? object_->GetHeadstartTime() : 0.0;

    movingDirection_     = 1;  // init to forward motion, will be re-evalutated for non-timestamp modes
    int headingDirection = (GetAbsAngleDifference(object_->pos_.GetH(), traj_->GetHTrue()) > M_PI_2 + SMALL_NUMBER) ? -1 : 1;

    // Move along trajectory
    if (
        // Ignore any timing info in trajectory
        timing_domain_ == TimingDomain::NONE ||
        // Speed is controlled elsewhere - just follow trajectory with current speed
        (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG))))
    {
        // determine moving direction based on segment inital heading, in addition to speed sign
        movingDirection_ = SIGN(object_->GetSpeed()) * initialHeadingSign_;
        object_->pos_.MoveTrajectoryDS(movingDirection_ * fabs(object_->speed_) * dt);
    }
    else if (timing_domain_ == TimingDomain::TIMING_RELATIVE)
    {
        time_ += timing_scale_ * dt;
        object_->pos_.SetTrajectoryPosByTime(time_ + timing_offset_);

        // calculate and update actual speed only while not reached end of trajectory,
        // since the movement is based on remaining length of trajectory, not speed
        if (time_ + timing_offset_ < traj_->GetStartTime() + traj_->GetDuration() + SMALL_NUMBER)
        {
            if (dt > SMALL_NUMBER)  // only update speed if some time has passed
            {
                object_->SetSpeed(movingDirection_ * headingDirection * fabs(object_->pos_.GetTrajectoryS() - old_s) / dt);
            }
        }
    }
    else if (timing_domain_ == TimingDomain::TIMING_ABSOLUTE)
    {
        if (object_->IsGhost() || simTime > -SMALL_NUMBER)
        {
            time_ = (simTime + dt) * timing_scale_;
        }

        object_->pos_.SetTrajectoryPosByTime(time_ + timeOffset + timing_offset_);

        if ((dt > SMALL_NUMBER) &&  // skip speed update if timestep is zero
            (time_ + timeOffset < traj_->GetStartTime() + traj_->GetDuration() + SMALL_NUMBER))
        {
            // don't calculate and update actual speed when reached end of trajectory,
            // since the movement is based on remaining length of trajectory, not speed
            object_->SetSpeed(movingDirection_ * headingDirection * fabs(object_->pos_.GetTrajectoryS() - old_s) / dt);
        }
    }

    // Check if switching into segment with no specified heading
    if (traj_->IsHSetExplicitly())
    {
        explicit_h_active_ = true;
    }
    else
    {
        if (explicit_h_active_)
        {
            // entering a segment lacking specified heading, determine moving direction based on relative heading
            initialHeadingSign_ = GetAbsAngleDifference(object_->pos_.GetH(), traj_->GetHTrue()) > M_PI_2 + SMALL_NUMBER ? -1 : 1;
            explicit_h_active_  = false;
        }

        // adjust entity heading
        if (initialHeadingSign_ < 0)
        {
            object_->pos_.SetHeading(GetAngleInInterval2PI(object_->pos_.GetH() + M_PI));
        }
    }
}

void FollowTrajectoryAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }
    if (traj_->shape_->type_ == roadmanager::Shape::ShapeType::CLOTHOID)
    {
        roadmanager::ClothoidShape* cl = static_cast<roadmanager::ClothoidShape*>(traj_->shape_);
        cl->pos_.ReplaceObjectRefs(&obj1->pos_, &obj2->pos_);
    }
    else if (traj_->shape_->type_ == roadmanager::Shape::ShapeType::POLYLINE)
    {
        roadmanager::PolyLineShape* pl = static_cast<roadmanager::PolyLineShape*>(traj_->shape_);
        for (size_t i = 0; i < pl->vertex_.size(); i++)
        {
            pl->vertex_[i].pos_->ReplaceObjectRefs(&obj1->pos_, &obj2->pos_);
        }
    }
}

AcquirePositionAction::~AcquirePositionAction()
{
    object_->pos_.SetRoute(nullptr);
    if (route_ != nullptr)
    {
        delete route_;
        route_ = nullptr;
    }
}

void AcquirePositionAction::Start(double simTime)
{
    // Resolve route
    if (route_ != nullptr)
    {
        delete route_;
        route_ = nullptr;
    }
    route_ = new roadmanager::Route;
    route_->setName("AcquirePositionRoute");
    route_->setObjName(object_->GetName());

    // find route from current position and considering current heading direction
    roadmanager::Position start_wp(object_->pos_);
    start_wp.SetHeadingRelative(fabs(GetAngleInIntervalMinusPIPlusPI(object_->pos_.GetHRelative())) > M_PI_2 ? M_PI : 0.0);
    start_wp.SetModeBits(roadmanager::Position::PosModeType::INIT, roadmanager::Position::PosMode::H_REL | roadmanager::Position::PosMode::H_SET);
    route_->AddWaypoint(start_wp);
    route_->AddWaypoint(target_position_);

    object_->pos_.SetRoute(route_);
    object_->SetDirtyBits(Object::DirtyBit::ROUTE);

    OSCAction::Start(simTime);

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }
}

void AcquirePositionAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

void AcquirePositionAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }
}

void AssignControllerAction::Start(double simTime)
{
    if (controller_)
    {
        if (object_ != nullptr)
        {
            object_->AssignController(controller_);
        }

        if (controller_)
        {
            if (!controller_->Active())
            {
                if (lat_activation_mode_ != ControlActivationMode::UNDEFINED || long_activation_mode_ != ControlActivationMode::UNDEFINED ||
                    light_activation_mode_ != ControlActivationMode::UNDEFINED || anim_activation_mode_ != ControlActivationMode::UNDEFINED)
                {
                    controller_->Activate({lat_activation_mode_, long_activation_mode_, light_activation_mode_, anim_activation_mode_});
                    LOG_INFO("Controller {} activated (lat {}, long {}, light {}, anim {}), domain mask=0x{}",
                             controller_->GetName(),
                             DomainActivation2Str(lat_activation_mode_),
                             DomainActivation2Str(long_activation_mode_),
                             DomainActivation2Str(light_activation_mode_),
                             DomainActivation2Str(anim_activation_mode_),
                             controller_->GetActiveDomains());
                }
            }
            else
            {
                LOG_INFO("Controller {} already active (domainmask 0x{}), should not happen when just assigned!",
                         controller_->GetName(),
                         controller_->GetActiveDomains());
            }
        }
    }

    OSCAction::Start(simTime);
}

scenarioengine::ActivateControllerAction::ActivateControllerAction(std::string           ctrl_name,
                                                                   ControlActivationMode lat_activation_mode,
                                                                   ControlActivationMode long_activation_mode,
                                                                   ControlActivationMode light_activation_mode,
                                                                   ControlActivationMode anim_activation_mode,
                                                                   StoryBoardElement*    parent)
    : OSCPrivateAction(OSCPrivateAction::ActionType::ACTIVATE_CONTROLLER, parent, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_NONE)),
      ctrl_name_(ctrl_name),
      controller_(nullptr)
{
    activation_mode_[static_cast<unsigned int>(ControlDomains::DOMAIN_LAT)]   = lat_activation_mode;
    activation_mode_[static_cast<unsigned int>(ControlDomains::DOMAIN_LONG)]  = long_activation_mode;
    activation_mode_[static_cast<unsigned int>(ControlDomains::DOMAIN_ANIM)]  = anim_activation_mode;
    activation_mode_[static_cast<unsigned int>(ControlDomains::DOMAIN_LIGHT)] = light_activation_mode;
}

void ActivateControllerAction::Start(double simTime)
{
    if (object_->controllers_.size() == 0)
    {
        LOG_INFO("No controller assigned to object {}!", object_->name_);
    }
    else
    {
        controller_ = nullptr;

        if (ctrl_name_.empty())
        {
            controller_ = object_->controllers_.back();

            if (scenarioEngine_->GetScenarioReader()->GetVersionMinor() > 2)
            {
                LOG_WARN("Warning: No controller name given for activation on object {}, {}: {}",
                         object_->name_,
                         object_->GetNrOfAssignedControllers() > 1 ? "pick most recently assigned" : "using assigned controller",
                         controller_->GetName());
            }
        }
        else
        {
            controller_ = object_->GetController(ctrl_name_);
            if (controller_ == nullptr)
            {
                LOG_ERROR("Error: Controller {} is not assigned to object {}", ctrl_name_, object_->GetName());
            }
        }

        if (controller_ != nullptr)
        {
            // first deactivate any controller active on the requested domain(s)
            for (unsigned int i = 0; i < static_cast<unsigned int>(ControlDomains::COUNT); i++)
            {
                Controller*    ctrl   = nullptr;
                ControlDomains domain = static_cast<ControlDomains>(i);
                if (activation_mode_[i] == ControlActivationMode::ON &&
                    (ctrl = object_->GetControllerActiveOnDomainMask(ControlDomain2DomainMask(domain))) != nullptr)
                {
                    if (scenarioEngine_->GetScenarioReader()->GetVersionMinor() >= 3)
                    {
                        LOG_WARN("Deactivating ctrl {} on domain {} (>= osc v1.3)", controller_->GetName(), ControlDomain2Str(domain));
                        // from osc v1.3 onwards, deactivation is done per domain
                        controller_->DeactivateDomains(static_cast<unsigned int>(ControlDomain2DomainMask(domain)));
                    }
                    else
                    {
                        // prior to osc v1.3 only one controller can be active, deactivate current active controller - which is also the only one
                        LOG_WARN("Deactivating ctrl {} conflicting on domain {} (< osc v1.3)", ctrl->GetName(), ControlDomain2Str(domain));
                        ctrl->Deactivate();
                        break;
                    }
                }
            }

            if (controller_->Activate(activation_mode_) == 0)
            {
                object_->SetDirtyBits(Object::DirtyBit::CONTROLLER);
                LOG_INFO("Controller {} active on domains: {} (mask=0x{})",
                         controller_->GetName(),
                         ControlDomainMask2Str(controller_->GetActiveDomains()),
                         controller_->GetActiveDomains());
                OSCAction::Start(simTime);
                End();  // action ends immediately
            }
            else
            {
                LOG_ERROR("Error: Failed to activate controller {} assigned to object {}!", controller_->GetName(), object_->GetName());
            }
        }
    }
}

void ActivateControllerAction::End()
{
    OSCAction::End();
}

void LatLaneChangeAction::Start(double simTime)
{
    OSCAction::Start(simTime);
    int target_lane_id_ = 0;

    transition_.Reset();

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }

    if (target_->type_ == Target::Type::ABSOLUTE_LANE)
    {
        target_lane_id_ = target_->value_;
    }
    else if (target_->type_ == Target::Type::RELATIVE_LANE)
    {
        // Find out target lane relative referred vehicle
        Object* ref_entity = (static_cast<TargetRelative*>(target_))->object_;
        if (ref_entity != nullptr)
        {
            target_lane_id_ = ref_entity->pos_.GetLaneId() + target_->value_ * (IsAngleForward(ref_entity->pos_.GetHRelative()) ? 1 : -1);

            if (target_lane_id_ == 0 || SIGN(ref_entity->pos_.GetLaneId()) != SIGN(target_lane_id_))
            {
                // Skip reference lane (id == 0)
                target_lane_id_ = SIGN(target_lane_id_ - ref_entity->pos_.GetLaneId()) * (abs(target_lane_id_) + 1);
            }
        }
        else
        {
            LOG_WARN("LaneChange RelativeTarget ref entity not found!");
        }
    }

    // Reset orientation, align to road
    object_->pos_.SetHeadingRelativeRoadDirection(0.0);
    object_->pos_.SetPitchRelative(0.0);
    object_->pos_.SetRollRelative(0.0);

    // Set initial state
    auto org_lane_id = object_->pos_.GetLaneId();
    object_->pos_.ForceLaneId(target_lane_id_);
    internal_pos_ = object_->pos_;

    // Make offsets agnostic to lane sign
    transition_.SetStartVal(SIGN(object_->pos_.GetLaneId()) * object_->pos_.GetOffset());
    transition_.SetTargetVal(SIGN(target_lane_id_) * target_lane_offset_);

    // Assign lane_id back to original lane_id, avoid assigned lane id spiking during start of lane change
    object_->pos_.ForceLaneId(org_lane_id);
}

void LatLaneChangeAction::Step(double simTime, double dt)
{
    double offset_agnostic = internal_pos_.GetOffset() * SIGN(internal_pos_.GetLaneId());

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }

    // signal that an action owns control
    object_->SetDirtyBits(Object::DirtyBit::LATERAL | Object::DirtyBit::LONGITUDINAL);

    if (!object_->IsGhost() && simTime < 0.0)
    {
        // Only ghosts are moving up to and including time == 0
        return;
    }

    if (abs(object_->GetSpeed()) < SMALL_NUMBER)
    {
        return;
    }

    double rate       = transition_.EvaluateScaledPrim();
    double step_len   = fabs(object_->speed_) * dt;  // travel distance total
    double delta_long = 0.0;
    if (transition_.dimension_ == DynamicsDimension::TIME || transition_.dimension_ == DynamicsDimension::RATE)
    {
        // ds^2 = steplen^2 - dt^2
        double delta_lat = dt * fabs(rate);
        delta_long       = sqrt(pow(step_len, 2.0) - pow(MIN(delta_lat, step_len), 2.0));

        if (delta_lat < step_len + SMALL_NUMBER)
        {
            // requested delta lateral motion is less than total step length
            transition_.Step(dt);
            offset_agnostic   = transition_.Evaluate();
            heading_agnostic_ = GetAngleInInterval2PI(atan2(SIGN(object_->speed_) * SIGN(rate) * delta_lat, delta_long));
        }
        else
        {
            // requested lateral motion too large wrt speed, skip long motion and limit lateral one
            // fall back to linear interpolation
            transition_.Step(step_len * transition_.GetParamTargetVal() / abs(transition_.GetTargetVal() - transition_.GetStartVal()));
            if (!NEAR_NUMBERS(heading_agnostic_, SIGN(object_->speed_) * (M_PI_2 - SMALL_NUMBER)) && transition_.shape_ != DynamicsShape::LINEAR)
            {
                LOG_WARN("LatLaneChangeAction: Limiting lateral motion and falling back to linear interpolation");
            }
            offset_agnostic   = transition_.Evaluate(DynamicsShape::LINEAR);
            heading_agnostic_ = SIGN(object_->speed_) * SIGN(rate) * (M_PI_2 - SMALL_NUMBER);  // avoid ambiguous driving direction
        }
    }
    else if (transition_.dimension_ == DynamicsDimension::DISTANCE)
    {
        delta_long = sqrt(pow(step_len, 2.0) / (1.0 + pow(rate, 2.0)));
        transition_.Step(delta_long);
        offset_agnostic   = transition_.Evaluate();
        heading_agnostic_ = atan(rate);  // heading for the step movement
    }
    else
    {
        LOG_ERROR("Unexpected, unsupported transition dimension: {}", transition_.dimension_);
    }

    // Restore position to target lane and new offset
    object_->pos_.SetLanePos(internal_pos_.GetTrackId(),
                             internal_pos_.GetLaneId(),
                             internal_pos_.GetS(),
                             offset_agnostic * SIGN(internal_pos_.GetLaneId()));

    if (transition_.shape_ == DynamicsShape::STEP ||
        (transition_.dimension_ != DynamicsDimension::RATE && transition_.GetParamTargetVal() < SMALL_NUMBER))
    {
        // not for step shape, since it is not a continuous function. Maintain longitudinal motion
        delta_long = step_len;
    }

    double ds = object_->pos_.DistanceToDS(SIGN(object_->speed_) * delta_long);  // find correspondning delta s along road reference line

    object_->pos_.MoveAlongS(ds, 0.0, -1.0, false, roadmanager::Position::MoveDirectionMode::HEADING_DIRECTION, true);
    internal_pos_.SetLanePos(object_->pos_.GetTrackId(), object_->pos_.GetLaneId(), object_->pos_.GetS(), object_->pos_.GetOffset());

    if (object_->pos_.GetRoute())
    {
        // Check whether updated position still is on the route, i.e. same road ID, or not
        if (!object_->pos_.IsInJunction() && !internal_pos_.GetTrackId() && object_->pos_.GetTrackId() != internal_pos_.GetTrackId())
        {
            LOG_WARN("Warning/Info: LaneChangeAction moved away from route (track id {} -> track id {}), disabling route",
                     object_->pos_.GetTrackId(),
                     internal_pos_.GetTrackId());
            object_->pos_.SetRoute(nullptr);
            object_->SetDirtyBits(Object::DirtyBit::ROUTE);
        }
    }

    if (transition_.GetParamVal() > transition_.GetParamTargetVal() - SMALL_NUMBER ||
        // Close enough?
        fabs(offset_agnostic - transition_.GetTargetVal()) < SMALL_NUMBER ||
        // Passed target value?
        (transition_.GetParamVal() > 0 &&
         SIGN(offset_agnostic - transition_.GetTargetVal()) != SIGN(transition_.GetStartVal() - transition_.GetTargetVal())))
    {
        OSCAction::End();
        object_->pos_.SetHeadingRelativeRoadDirection(0);
    }
    else
    {
        object_->pos_.SetHeadingRelativeRoadDirection((IsAngleForward(object_->pos_.GetHRelative()) ? 1 : -1) * SIGN(object_->pos_.GetLaneId()) *
                                                      heading_agnostic_);
    }

    if (!(object_->pos_.GetRoute() && object_->pos_.GetRoute()->IsValid()))
    {
        // Attach object position to closest road and lane, look up via inertial coordinates
        object_->pos_.XYZ2TrackPos(object_->pos_.GetX(), object_->pos_.GetY(), object_->pos_.GetZ(), roadmanager::Position::PosMode::Z_ABS);
    }

    if (object_->pos_.GetStatusBitMask() & static_cast<int>(roadmanager::Position::PositionStatusMode::POS_STATUS_END_OF_ROAD))
    {
        OSCAction::End();
    }

    object_->SetDirtyBits(Object::DirtyBit::LATERAL | Object::DirtyBit::LONGITUDINAL | Object::DirtyBit::SPEED);
}

void LatLaneChangeAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (target_->type_ == Target::Type::RELATIVE_LANE)
    {
        if ((static_cast<TargetRelative*>(target_))->object_ == obj1)
        {
            (static_cast<TargetRelative*>(target_))->object_ = obj2;
        }
    }
}

void LatLaneOffsetAction::Start(double simTime)
{
    OSCAction::Start(simTime);
    transition_.Reset();

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }

    if (target_->type_ == Target::Type::ABSOLUTE_OFFSET)
    {
        transition_.SetTargetVal(SIGN(object_->pos_.GetLaneId()) * target_->value_);
    }
    else if (target_->type_ == Target::Type::RELATIVE_OFFSET)
    {
        // Register what lane action object belongs to
        int lane_id = object_->pos_.GetLaneId();

        // Find out target position based on the referred object
        roadmanager::Position refpos = (static_cast<TargetRelative*>(target_.get()))->object_->pos_;
        refpos.SetTrackPos(refpos.GetTrackId(), refpos.GetS(), refpos.GetT() + target_->value_ * (IsAngleForward(refpos.GetHRelative()) ? 1 : -1));

        // Transform target position into lane position based on current lane id
        refpos.ForceLaneId(lane_id);

        // Target lane offset = target t value - t value of current lane (which is current t - current offset)
        transition_.SetTargetVal(SIGN(object_->pos_.GetLaneId()) * (refpos.GetT() - (object_->pos_.GetT() - object_->pos_.GetOffset())));
    }

    transition_.SetStartVal(SIGN(object_->pos_.GetLaneId()) * object_->pos_.GetOffset());
    transition_.SetParamTargetVal(transition_.GetTargetParamValByPrimPrimPeak(max_lateral_acc_));
}

void LatLaneOffsetAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;
    double offset_agnostic;

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }

    offset_agnostic = transition_.Evaluate();

    if (transition_.GetParamVal() > transition_.GetParamTargetVal() - SMALL_NUMBER ||
        // Close enough?
        fabs(offset_agnostic - transition_.GetTargetVal()) < SMALL_NUMBER ||
        // Passed target value?
        (transition_.GetParamVal() > 0 &&
         SIGN(offset_agnostic - transition_.GetTargetVal()) != SIGN(transition_.GetStartVal() - transition_.GetTargetVal())))
    {
        OSCAction::End();
        object_->pos_.SetLanePos(object_->pos_.GetTrackId(),
                                 object_->pos_.GetLaneId(),
                                 object_->pos_.GetS(),
                                 SIGN(object_->pos_.GetLaneId()) * transition_.GetTargetVal());
        object_->pos_.SetHeadingRelativeRoadDirection(0);
    }
    else
    {
        object_->pos_.SetLanePos(object_->pos_.GetTrackId(),
                                 object_->pos_.GetLaneId(),
                                 object_->pos_.GetS(),
                                 SIGN(object_->pos_.GetLaneId()) * offset_agnostic);

        // Convert rate (lateral-movment/time) to lateral-movement/long-movement
        double angle = atan(transition_.EvaluatePrim() / AVOID_ZERO(object_->GetSpeed()));
        object_->pos_.SetHeadingRelativeRoadDirection((IsAngleForward(object_->pos_.GetHRelative()) ? 1 : -1) * SIGN(object_->pos_.GetLaneId()) *
                                                      angle);
    }

    object_->SetDirtyBits(Object::DirtyBit::LATERAL);

    transition_.Step(dt);
}

void LatLaneOffsetAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (target_->type_ == Target::Type::RELATIVE_OFFSET)
    {
        if ((static_cast<TargetRelative*>(target_.get()))->object_ == obj1)
        {
            (static_cast<TargetRelative*>(target_.get()))->object_ = obj2;
        }
    }
}
double LongSpeedAction::TargetRelative::GetValue()
{
    double object_speed = object_ ? object_->speed_ : 0.0;

    if (value_type_ == ValueType::DELTA)
    {
        return object_speed + value_;
    }
    else if (value_type_ == ValueType::FACTOR)
    {
        return object_speed * value_;
    }
    else
    {
        LOG_ERROR("Invalid value type: {}", value_type_);
    }

    return 0;
}

void LongSpeedAction::Start(double simTime)
{
    OSCAction::Start(simTime);
    transition_.Reset();
    target_speed_reached_ = false;

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG)))
    {
        // longitudinal motion controlled elsewhere
        OSCAction::End();
        return;
    }

    transition_.SetStartVal(object_->GetSpeed());

    if (transition_.dimension_ == DynamicsDimension::DISTANCE)
    {
        // Convert to time, since speed shape is expected over time, not distance (as in lane change case)
        // special cased when speed changes sign:

        double v0   = transition_.GetStartVal();
        double v1   = target_->GetValue();
        double dist = transition_.GetParamTargetVal();

        if (abs(v1 - v0) < SMALL_NUMBER)
        {
            // no change
            transition_.SetParamTargetVal(0.0);
        }
        else if (abs(v0) > SMALL_NUMBER && abs(v1) > SMALL_NUMBER && SIGN(v0) != SIGN(v1))
        {
            // sign of speed changes, add two parts divided by speed = 0
            // find relation dist0 / (dist0 + dist1) = v0^2 / (v0^2 + v1^2)
            double dist_factor = abs(pow(v0, 2) / (pow(v0, 2) + pow(v1, 2)));

            // Find displacement of part 1
            // d0 = (v0 * t0) / 2 => t0 = (2 * d0) / v0
            double d0 = dist_factor * dist;

            // calculate duration of part 1
            double t0 = 2.0 * d0 / abs(v0);

            // calculate duration of remaning part 2
            double t1 = 2.0 * (dist - d0) / abs(v1);

            transition_.SetParamTargetVal(t0 + t1);
        }
        else  // change of speed without changing driving direction
        {
            // integrated distance = time(v_init + v_delta/2) = time(v_init + v_end)/2
            // => time = 2*distance/(v_init + v_end)
            transition_.SetParamTargetVal(2 * dist / (abs(v0 + v1)));
        }
    }

    transition_.SetTargetVal(ABS_LIMIT(target_->GetValue(), object_->performance_.maxSpeed));

    if (transition_.GetTargetVal() > transition_.GetStartVal())
    {
        // Acceleration
        transition_.SetMaxRate(object_->performance_.maxAcceleration);
    }
    else
    {
        // Deceleration
        transition_.SetMaxRate(object_->performance_.maxDeceleration);
    }

    // Make sure sign of rate is correct
    if (transition_.dimension_ == DynamicsDimension::RATE)
    {
        transition_.SetRate(SIGN(transition_.GetTargetVal() - transition_.GetStartVal()) * abs(transition_.GetRate()));
    }

    // Set initial state
    object_->SetSpeed(transition_.Evaluate());
}

void LongSpeedAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;
    double new_speed = 0;

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG)))
    {
        // longitudinal motion controlled elsewhere
        OSCAction::End();
        return;
    }

    if (target_->type_ == Target::TargetType::RELATIVE_SPEED)
    {
        transition_.SetTargetVal(ABS_LIMIT(target_->GetValue(), object_->performance_.maxSpeed));
    }

    if (transition_.GetParamTargetVal() > 0.0 && (transition_.GetParamVal() > transition_.GetParamTargetVal() - SMALL_NUMBER))
    {
        double acc = dt > SMALL_NUMBER ? (target_->GetValue() - object_->GetSpeed()) / dt : 0.0;

        // respect performance constraints after transition phase
        acc = CLAMP(acc, -object_->performance_.maxDeceleration, object_->performance_.maxAcceleration);

        new_speed = ABS_LIMIT(object_->GetSpeed() + acc * dt, object_->performance_.maxSpeed);
    }
    else
    {
        transition_.Step(dt);
        new_speed           = transition_.Evaluate();
        double acc_required = dt > SMALL_NUMBER ? (new_speed - object_->GetSpeed()) / dt : 0.0;

        // too big step?
        if (acc_required > object_->performance_.maxAcceleration + SMALL_NUMBER ||
            acc_required < -(object_->performance_.maxDeceleration + SMALL_NUMBER))
        {
            double acc = CLAMP(acc_required, -object_->performance_.maxDeceleration, object_->performance_.maxAcceleration);

            // find new speed based on limited acceleration
            new_speed = ABS_LIMIT(object_->GetSpeed() + acc * dt, object_->performance_.maxSpeed);

            // linear interpolation to find approximate step
            double step_approx = (new_speed - object_->GetSpeed()) / acc_required;

            // adjust transition step accordingly
            transition_.Step(-(dt - step_approx));
        }
    }

    if (transition_.GetParamVal() > transition_.GetParamTargetVal() - SMALL_NUMBER)
    {
        // Close enough?
        if (abs(new_speed - transition_.GetTargetVal()) < SMALL_NUMBER)
        {
            target_speed_reached_ = true;
            new_speed             = ABS_LIMIT(target_->GetValue(), object_->performance_.maxSpeed);
        }
    }

    object_->SetSpeed(new_speed);

    if (target_speed_reached_ &&
        !(target_->type_ == Target::TargetType::RELATIVE_SPEED && (static_cast<TargetRelative*>(target_.get()))->continuous_ == true))
    {
        OSCAction::End();
    }
}

void LongSpeedProfileAction::Start(double simTime)
{
    OSCAction::Start(simTime);

    if (entry_.size() == 0 || object_ == nullptr)
    {
        return;
    }

    double speed_offset = entity_ref_ != nullptr ? entity_ref_->GetSpeed() : 0.0;
    double init_time_   = simTime;

    init_acc_ = object_->pos_.GetAccLong();

    std::vector<EntryVertex> vertex;
    unsigned int             index = 0;

    // Check need for initial entry with current speed
    if (entry_[0].time_ > SMALL_NUMBER)
    {
        // insert initial entry at time = 0 with initial/current speed
        vertex.push_back(EntryVertex(init_time_, object_->GetSpeed()));
    }
    else if (entry_[0].time_ > -SMALL_NUMBER)
    {
        if (following_mode_ == FollowingMode::FOLLOW)
        {
            // replace first entry at time = 0.0 with current speed
            vertex.push_back(EntryVertex(init_time_, object_->GetSpeed()));
            index = 1;  // Skip first entry
        }
        else
        {
            // in linear mode use specified initial speed
            vertex.push_back(EntryVertex(init_time_, entry_[0].speed_));
        }
    }
    else  // negative time indicating time attribute omitted
    {
        // Create a first entry at current time and speed
        vertex.push_back(EntryVertex(init_time_, object_->GetSpeed()));
    }

    // First filter out any obsolete middle points on straight acceleration lines
    double delta_k, j = 0.0, time = init_time_;
    for (; index < entry_.size(); index++)
    {
        if (entry_[index].time_ < -SMALL_NUMBER)
        {
            if (abs(entry_[index].speed_ + speed_offset - vertex.back().v_) < SMALL_NUMBER)
            {
                // same speed as previous vertex, skip entry
                continue;
            }
            // negative time is interpreted as missing time stamp
            double dv  = entry_[index].speed_ + speed_offset - vertex.back().v_;
            double acc = 0.0;
            if (dv < 0)
            {
                acc = MIN(-SMALL_NUMBER, -dynamics_.max_deceleration_);
            }
            else
            {
                acc = MAX(SMALL_NUMBER, dynamics_.max_acceleration_);
            }

            vertex.back().t_ = time;
            time += dv / acc;
        }
        else
        {
            time += entry_[index].time_;
        }

        double dt = time - vertex.back().t_;

        if (index < entry_.size() - 1)
        {
            if (dt < SMALL_NUMBER)
            {
                // replace previous entry
                vertex.back() = EntryVertex(time, entry_[index].speed_ + speed_offset);
                continue;  // skip entry
            }
        }

        vertex.back().SetK((entry_[index].speed_ + speed_offset - (vertex.back().v_)) / dt);

        if (index > 1)
        {
            delta_k = vertex.back().k_ - (vertex.rbegin() + 1)->k_;  // last - second last
        }
        else
        {
            delta_k = vertex.back().k_ - init_acc_;  // slope is zero at start - no delta
        }

        if (abs(delta_k) < SMALL_NUMBER && index > 1)
        {
            // replace previous entry
            double new_k  = (entry_[index].speed_ + speed_offset - (vertex.rbegin() + 1)->v_) / (time - (vertex.rbegin() + 1)->t_);
            vertex.back() = EntryVertex(time, entry_[index].speed_ + speed_offset, new_k);
            continue;  // skip entry
        }

        if (index == entry_.size() - 1)
        {
            // Final entry, set k = 0
            vertex.push_back(EntryVertex(time, entry_[index].speed_ + speed_offset, 0.0));
        }
        else
        {
            vertex.push_back(EntryVertex(time, entry_[index].speed_ + speed_offset));
        }
    }

    EntryVertex vtx = vertex[0];
    double      t0, t1, t3 = vtx.t_, v0, v3 = 0.0, m0, m1 = 0.0, k1 = 0.0;
    double      k0 = init_acc_;

    segment_.clear();

    // Some info on the implementation concept and equations systems can be found here:
    // https://drive.google.com/file/d/1DmjVHftcsbU71Ce_GASZ6IArcPA6teNF/view?usp=sharing

    if (vertex.size() == 1)
    {
        AddSpeedSegment(vtx.t_, vtx.v_, 0.0, 0.0);
    }
    else if (following_mode_ == FollowingMode::FOLLOW && vertex.size() == 2)
    {
        // Special case: Single speed target, following mode = follow
        // Reach target speed with given jerk contraint. Add linear segment if needed.

        // Set first jerk segment

        j = (vertex[0].k_ - k0) < 0 ? -dynamics_.max_deceleration_rate_ : dynamics_.max_acceleration_rate_;
        AddSpeedSegment(vtx.t_, vtx.v_, k0, j);

        double j0 = j;
        double j1 = 0.0, t2 = 0.0, v1, v2 = 0.0;

        // Find jerk at endpoint, where acceleration / k is zero
        if (vertex[0].k_ < 0)
        {
            j1 = dynamics_.max_acceleration_rate_;
        }
        else
        {
            j1 = -dynamics_.max_deceleration_rate_;
        }

        t0 = vertex[0].t_;
        v0 = vertex[0].v_;
        t3 = vertex[1].t_;
        v3 = vertex[1].v_;

        if (fabs(j1 - j0) < SMALL_NUMBER)
        {
            // following_mode_ = FollowingMode::POSITION;
            if (fabs(t0 - t3) > SMALL_NUMBER && fabs(j1 * (t0 - t3) - k0) > SMALL_NUMBER)
            {
                t1 = (2 * j1 * (v0 + t0 * j1 * (t0 - t3) - v3) + pow(k0, 2) + 2 * k0 * j1 * (t3 - 2 * t0)) / (2 * j1 * (j1 * (t0 - t3) - k0));
                if (t1 < 0)
                {
                    LOG_ERROR("SpeedProfile: No solution found (t1) - fallback to Position mode");
                }
                else
                {
                    v1 = v0 + k0 * (t1 - t0) + j0 * 0.5 * pow((t1 - t0), 2);
                    k1 = k0 + j0 * (t1 - t0);
                    t2 = (k1 / j1) + t3;
                    if (t2 < 0)
                    {
                        LOG_ERROR("SpeedProfile: No solution found (t2)");
                    }
                    else
                    {
                        v2 = v1 + k1 * (t2 - t1);
                        if (k1 < dynamics_.max_acceleration_ && k1 > -dynamics_.max_deceleration_)
                        {
                            // add linear segment
                            AddSpeedSegment(t1, v1, k1, 0.0);
                        }
                    }
                }
            }
            else
            {
                LOG_ERROR("SpeedProfile: No solution found (t3)");
            }
        }
        else
        {
            double factor = j0 * j1 * (2 * v0 * (j1 - j0) + pow(k0, 2) + 2 * k0 * j1 * (t3 - t0) + j0 * j1 * pow(t0 - t3, 2) + 2 * v3 * (j0 - j1));

            if (factor < 0.0)
            {
                // no room or time for linear segment of constant acceleration
                t2 = (sqrt(j1 * (-(j0 - j1)) * (j0 * (2 * v3 - 2 * v0) + pow(k0, 2))) + (j0 - j1) * (t0 * j0 - k0)) / (j0 * (j0 - j1));
                v2 = v0 + k0 * (t2 - t0) + j0 * 0.5 * pow(t2 - t0, 2);
                k1 = k0 + j0 * (t2 - t0);
                t3 = t2 - k1 / j1;

                if (IS_IN_SPAN(k1, -dynamics_.max_deceleration_, dynamics_.max_acceleration_) && t3 > vertex[1].t_)
                {
                    LOG_WARN("SpeedProfile: Can't reach target speed {:.2f} on target time {:.2f}s with given jerk constraints, extend to {:.2f}s",
                             v3,
                             vertex[1].t_,
                             t3);
                }
            }
            else
            {
                // need a linear segment to reach speed at specified time
                LOG_INFO("SpeedProfile: Add linear segment to reach target speed on time.");
                t1 = (-sqrt(factor) - j0 * (k0 + t3 * j1) + t0 * pow(j0, 2)) / (j0 * (j0 - j1));
                v1 = v0 + k0 * (t1 - t0) + j0 * 0.5 * pow(t1 - t0, 2);
                k1 = init_acc_ + j0 * (t1 - t0);
                t2 = (k1 / j1) + t3;
                v2 = v1 + k1 * (t2 - t1);

                if (IS_IN_SPAN(k1, -dynamics_.max_deceleration_, dynamics_.max_acceleration_))
                {
                    // add linear segment
                    AddSpeedSegment(t1, v1, k1, 0.0);
                }
            }
        }

        if (IS_IN_SPAN(k1, -dynamics_.max_deceleration_, dynamics_.max_acceleration_))
        {
            // add second jerk segment
            AddSpeedSegment(t2, v2, k1, j1);
            // add end segment
            AddSpeedSegment(t3, v3, 0.0, 0.0);
        }
        else
        {
            if (k1 > dynamics_.max_acceleration_)
            {
                LOG_INFO("SpeedProfile: Constraining acceleration from {:.2f} to {:.2f}", k1, dynamics_.max_acceleration_);
                k1 = dynamics_.max_acceleration_;
            }
            else if (k1 < -dynamics_.max_deceleration_)
            {
                LOG_INFO("SpeedProfile: Constraining deceleration from {:.2f} to {:.2f}", -k1, dynamics_.max_deceleration_);
                k1 = -dynamics_.max_deceleration_;
            }

            t1 = t0 + (k1 - k0) / j0;
            v1 = v0 + k0 * (t1 - t0) + 0.5 * j0 * pow(t1 - t0, 2);
            t2 = (v3 - v1) / k1 + t1 + k1 / (2 * j1);
            v2 = v3 + pow(k1, 2) / (2 * j1);
            t3 = -v1 / k1 + v3 / k1 + t1 - k1 / (2 * j1);

            LOG_INFO("SpeedProfile: Extend {:.2f} s", t3 - vertex.back().t_);

            // add linear segment
            AddSpeedSegment(t1, v1, k1, 0.0);
            // add second jerk segment
            AddSpeedSegment(t2, v2, k1, j1);
            // add end segment
            AddSpeedSegment(t3, v3, 0.0, 0.0);
        }
    }
    else
    {
        // Normal case: Follow acceleration (slopes) of multiple speed targets over time

        if (following_mode_ == FollowingMode::FOLLOW)
        {
            if (abs(vtx.k_) > SMALL_NUMBER)
            {
                j = (vertex[0].k_ - k0) < 0 ? -dynamics_.max_deceleration_rate_ : dynamics_.max_acceleration_rate_;
                AddSpeedSegment(vtx.t_, vtx.v_, k0, j);

                t3 = vtx.t_ + (abs(j) > SMALL_NUMBER ? (vtx.k_ - k0) / j : 0.0);  // duration of first jerk t = a / j
            }

            v3 = vtx.v_ + k0 * (t3 - init_time_) + 0.5 * j * pow((t3 - init_time_), 2);  // speed after initial jerk
            k1 = vtx.k_;                                                                 // slope of first linear segment
            m1 = v3 - k1 * t3;                                                           // eq constant for second acceleration line

            // add first linear segment
            AddSpeedSegment(t3, v3, k1, 0.0);
        }

        for (index = 0; index < vertex.size(); index++)
        {
            if (following_mode_ == FollowingMode::POSITION)
            {
                vtx = vertex[index];
                AddSpeedSegment(vtx.t_, vtx.v_, vtx.k_, 0.0);
            }
            else if (index < vertex.size() - 1)
            {
                k0  = k1;
                vtx = vertex[index + 1];
                t0  = t3;
                v0  = v3;
                m0  = m1;
                k1  = vtx.k_;
                m1  = vtx.m_;
                j   = vtx.k_ - k0 < 0 ? -dynamics_.max_deceleration_rate_ : dynamics_.max_acceleration_rate_;

                if (abs(k0 - k1) >= SMALL_NUMBER)
                {
                    // find time for next jerk
                    // https://www.wolframalpha.com/input?i=solve+b%3Dk*g%2Bn%2Cd%3Dl*j%2Bo%2Cd%3Db%2Bk*%28j-g%29%2Bm*%28j-g%29%5E2%2F2%2Cl%3Dk%2Bm*%28j-g%29+for+b%2Cd%2Cg%2Cj
                    // solve b = k * g + n, d = l * j + o, d = b + k * (j - g) + m * (j - g) ^ 2 / 2, l = k + m * (j - g) for b, d, g, j
                    // substitutions: a = v0, b = v1, c = v2, d = v3, f = t0, g = t1, h = t2, j = t3, k = k0, l = k1, m = j, n = m0, o = m1
                    // g = (k ^ 2 - 2 k l + l ^ 2 - 2 m(n - o)) / (2 m(k - l))
                    t1 = (pow(k0, 2) - 2 * k0 * k1 + pow(k1, 2) - 2 * j * (m0 - m1)) / (2 * j * (k0 - k1));

                    if (t1 < t0)
                    {
                        LOG_WARN("LongSpeedProfileAction failed at point {} (time={:.2f}). Falling back to linear (Position) mode.",
                                 index,
                                 vertex[index].t_);
                        following_mode_ = FollowingMode::POSITION;
                        continue;
                    }

                    double v1 = v0 + k0 * (t1 - t0);
                    t3        = t1 + (vtx.k_ - k0) / j;
                    v3        = v1 + k0 * (t3 - t1) + 0.5 * j * pow(t3 - t1, 2);

                    // add jerk segment
                    AddSpeedSegment(t1, v1, k0, j);
                }

                // add linear segment
                AddSpeedSegment(t3, v3, vtx.k_, 0.0);
            }
        }
    }

    cur_index_ = 0;
    speed_     = object_->GetSpeed();
}

void LongSpeedProfileAction::Step(double simTime, double dt)
{
    double time = simTime + dt;

    if (time < segment_.back().t + 10 && !(time > segment_.back().t and abs(speed_ - segment_.back().v) < SMALL_NUMBER))
    {
        while (static_cast<unsigned int>(cur_index_) < segment_.size() - 1 &&
               time > segment_[static_cast<unsigned int>(cur_index_) + 1].t - SMALL_NUMBER)
        {
            cur_index_++;
        }

        SpeedSegment* s = &segment_[static_cast<unsigned int>(cur_index_)];

        speed_ = s->v + s->k * (time - s->t) + 0.5 * s->j * pow(time - s->t, 2);
        if (NEAR_ZERO(speed_))
        {
            // avoid random jumping between positive and negative zero
            speed_ = 0.0;
        }
    }

    elapsed_ = MAX(0.0, time - segment_[0].t);

    if (static_cast<unsigned int>(cur_index_) >= entry_.size() - 1 && fabs(speed_ - segment_.back().v) < SMALL_NUMBER)
    {
        speed_ = segment_.back().v;
        OSCAction::End();
    }

    object_->SetSpeed(speed_);
}

void LongSpeedProfileAction::CheckAcceleration(double acc)
{
    if (following_mode_ == FollowingMode::POSITION)
    {
        return;
    }

    if (acc > dynamics_.max_acceleration_ + SMALL_NUMBER || acc < -dynamics_.max_deceleration_ - SMALL_NUMBER)
    {
        LOG_WARN("Acceleration {:.2f} not within constrained span [{:.2f}:{:.2f}], revert to linear mode",
                 acc,
                 dynamics_.max_acceleration_,
                 -dynamics_.max_deceleration_);

        following_mode_ = FollowingMode::POSITION;
    }
}

void LongSpeedProfileAction::CheckAccelerationRate(double acc_rate)
{
    if (following_mode_ == FollowingMode::POSITION)
    {
        return;
    }

    if (acc_rate > dynamics_.max_acceleration_rate_ + SMALL_NUMBER || acc_rate < -dynamics_.max_deceleration_rate_ - SMALL_NUMBER)
    {
        LOG_WARN("Acceleration rate {:.2f} not within constrained span [{:.2f}:{:.2f}], revert to linear mode",
                 acc_rate,
                 dynamics_.max_acceleration_rate_,
                 -dynamics_.max_deceleration_rate_);

        following_mode_ = FollowingMode::POSITION;
    }
}

void LongSpeedProfileAction::CheckSpeed(double speed)
{
    if (following_mode_ == FollowingMode::POSITION)
    {
        return;
    }

    if (speed > dynamics_.max_speed_ + SMALL_NUMBER || speed < -dynamics_.max_speed_ - SMALL_NUMBER)
    {
        LOG_WARN("Speed {:.2f} not within constrained span [{:.2f}:{:.2f}], revert to linear mode",
                 speed,
                 dynamics_.max_speed_,
                 -dynamics_.max_speed_);

        following_mode_ = FollowingMode::POSITION;
    }
}

void LongSpeedProfileAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (entity_ref_ == obj1)
    {
        entity_ref_ = obj2;
    }
}

void LongSpeedProfileAction::AddSpeedSegment(double t, double v, double k, double j)
{
    CheckSpeed(v);
    CheckAcceleration(k);
    CheckAccelerationRate(j);
    segment_.push_back({t, v, k, j});
}

void LongSpeedAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (target_->type_ == Target::TargetType::RELATIVE_SPEED)
    {
        if ((static_cast<TargetRelative*>(target_.get()))->object_ == obj1)
        {
            (static_cast<TargetRelative*>(target_.get()))->object_ = obj2;
        }
    }
}

void LongDistanceAction::Start(double simTime)
{
    if (target_object_ == 0)
    {
        LOG_ERROR("Can't trig without set target object ");
        return;
    }

    // Resolve displacement
    if (displacement_ == DisplacementType::ANY)
    {
        // Find out current displacement, and apply it
        double distance;
        if (freespace_)
        {
            double latDist  = 0;
            double longDist = 0;
            object_->FreeSpaceDistance(target_object_, &latDist, &longDist);
            distance = longDist;
        }
        else
        {
            double x, y;
            object_->pos_.getRelativeDistance(target_object_->pos_.GetX(), target_object_->pos_.GetY(), x, y);
            // Just interested in the x-axis component of the distance
            distance = x;
        }

        if (distance < 0.0)
        {
            displacement_ = DisplacementType::LEADING;
        }
        else
        {
            displacement_ = DisplacementType::TRAILING;
        }
    }

    OSCAction::Start(simTime);
}

void LongDistanceAction::Step(double simTime, double dt)
{
    (void)simTime;

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG)))
    {
        // longitudinal motion controlled elsewhere
        return;
    }

    // Find out current distance
    double distance;
    if (freespace_)
    {
        double latDist  = 0;
        double longDist = 0;
        object_->FreeSpaceDistance(target_object_, &latDist, &longDist);
        distance = longDist;
    }
    else
    {
        object_->pos_.Distance(&target_object_->pos_, cs_, roadmanager::RelativeDistanceType::REL_DIST_LONGITUDINAL, distance);
    }

    double speed_diff      = object_->speed_ - target_object_->speed_;
    double spring_constant = 0.4;
    double requested_dist  = 0;

    if (dist_type_ == DistType::DISTANCE)
    {
        requested_dist = distance_;
    }
    if (dist_type_ == DistType::TIME_GAP)
    {
        // Convert requested time gap (seconds) to distance (m)
        requested_dist = abs(target_object_->speed_) * distance_;
    }

    if (displacement_ == DisplacementType::TRAILING)
    {
        requested_dist = abs(requested_dist);
    }
    else if (displacement_ == DisplacementType::LEADING)
    {
        requested_dist = -abs(requested_dist);
    }

    double distance_diff = distance - requested_dist;

    if (continuous_ == false && fabs(distance_diff) < LONGITUDINAL_DISTANCE_THRESHOLD)
    {
        // Reached requested distance, quit action
        OSCAction::End();
    }

    if (dynamics_.max_acceleration_ >= LARGE_NUMBER && dynamics_.max_deceleration_ >= LARGE_NUMBER)
    {
        // Set position according to distance and copy speed of target vehicle
        object_->pos_.MoveAlongS(distance_diff);
        object_->SetSpeed(target_object_->speed_);
    }
    else
    {
        // Apply damped spring model with critical/optimal damping factor
        // Adjust tension in spring in proportion to the max acceleration and max deceleration
        double tension = distance_diff < 0.0 ? dynamics_.max_acceleration_ : dynamics_.max_deceleration_;

        double spring_constant_adjusted = tension * spring_constant;
        double dc                       = 2 * sqrt(spring_constant_adjusted);
        double acc                      = distance_diff * spring_constant_adjusted - speed_diff * dc;
        double jerk;
        if (acc < 0.0)
        {
            jerk = -dynamics_.max_deceleration_rate_;
            if (acc < -dynamics_.max_deceleration_)
            {
                acc = -dynamics_.max_deceleration_;
            }
        }
        else
        {
            jerk = dynamics_.max_acceleration_rate_;
            if (acc > dynamics_.max_acceleration_)
            {
                acc = dynamics_.max_acceleration_;
            }
        }

        // Apply simple linear model for jerk
        if (jerk < 0.0 && acc < acceleration_)
        {
            acc = MAX(acceleration_ + jerk * dt, acc);
        }
        else if (jerk > 0.0 && acc > acceleration_)
        {
            acc = MIN(acceleration_ + jerk * dt, acc);
        }

        acceleration_ = acc;
        object_->SetSpeed(object_->GetSpeed() + acceleration_ * dt);

        if (object_->GetSpeed() > dynamics_.max_speed_)
        {
            object_->SetSpeed(dynamics_.max_speed_);
        }
        else if (object_->GetSpeed() < -dynamics_.max_speed_)
        {
            object_->SetSpeed(-dynamics_.max_speed_);
        }
    }
}

void LongDistanceAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (target_object_ == obj1)
    {
        target_object_ = obj2;
    }
}

void LatDistanceAction::Start(double simTime)
{
    if (target_object_ == 0)
    {
        LOG_ERROR("Can't trig without set target object ");
        return;
    }

    // Resolve displacement
    if (displacement_ == DisplacementType::ANY)
    {
        LOG_INFO("LateralDistanceAction: Displacement not set, defaulting to leftToReferencedEntity");
        displacement_ = DisplacementType::LEFT_TO_REFERENCED_ENTITY;
    }

    double ego_width    = object_->boundingbox_.dimensions_.width_;
    double target_width = target_object_->boundingbox_.dimensions_.width_;

    if (cs_ == roadmanager::CoordinateSystem::CS_ENTITY)
    {
        if (freespace_)
        {
            distance_ += (ego_width + target_width) / 2.0;
        }
        distance_ *= displacement_ == DisplacementType::RIGHT_TO_REFERENCED_ENTITY ? -1 : 1;
    }
    else if (cs_ == roadmanager::CoordinateSystem::CS_ROAD)
    {
        if (displacement_ == DisplacementType::LEFT_TO_REFERENCED_ENTITY)
        {
            // We want to be on the left side of the target object
            distance_ = -abs(distance_);
            sign_     = -1.0;
        }
        else if (displacement_ == DisplacementType::RIGHT_TO_REFERENCED_ENTITY)
        {
            // We want to be on the right side of the target object
            distance_ = abs(distance_);
            sign_     = 1.0;
        }

        if (freespace_)
        {
            // We only take the width of the cars into account if we are in freespace mode
            distance_ += sign_ * (ego_width + target_width) / 2.0;
        }
    }

    move_state_ = MoveState::INIT;

    // do some usage checks
    if (cs_ == roadmanager::CoordinateSystem::CS_ENTITY)
    {
        if (dynamics_.max_acceleration_rate_ < LARGE_NUMBER || dynamics_.max_deceleration_rate_ < LARGE_NUMBER ||
            dynamics_.max_deceleration_ < LARGE_NUMBER || dynamics_.max_speed_ < LARGE_NUMBER)
        {
            LOG_WARN("LatDistanceAction: For entity coord system only maxAcceleration is supported (clamped at 1000). Omit for instant positioning.");
        }
    }
    else if (cs_ == roadmanager::CoordinateSystem::CS_ROAD)
    {
        if (dynamics_.max_acceleration_rate_ < LARGE_NUMBER || dynamics_.max_deceleration_rate_ < LARGE_NUMBER ||
            dynamics_.max_deceleration_ < LARGE_NUMBER)
        {
            LOG_INFO("LatDistanceAction: For road coord system only maxAcceleration and mAxSpeed are supported. Omit for instant positioning.");
        }
    }
    else
    {
        LOG_WARN("LatDistanceAction: Unsupported coordinate system {}", cs_);
    }

    OSCAction::Start(simTime);
}

void LatDistanceAction::GetDesiredRoadPos(const double distance_error, roadmanager::Position& internal_pos)
{
    double shift_x = 0.0;
    double shift_y = 0.0;
    if (cs_ == roadmanager::CoordinateSystem::CS_ROAD)
    {
        // How much we need to move to reach the target distance in the direction of the road
        shift_x = -std::sin(object_->pos_.GetRoadH()) * distance_error;
        shift_y = std::cos(object_->pos_.GetRoadH()) * distance_error;
    }
    else
    {
        LOG_WARN("LateralDistanceAction: Unsupported coordinate system");
    }

    // Add the necessary shift to the current position
    double target_x = object_->pos_.GetX() + shift_x;
    double target_y = object_->pos_.GetY() + shift_y;
    internal_pos.XYZ2TrackPos(target_x, target_y, object_->pos_.GetZ());
}

void LatDistanceAction::GetDistanceError(roadmanager::Position& pos1, roadmanager::Position& pos2, double& distance_error)
{
    // Find out current distance
    if (roadmanager::CoordinateSystem::CS_ROAD != cs_ && roadmanager::CoordinateSystem::CS_ENTITY != cs_)
    {
        LOG_WARN("LatDistanceAction: Unsupported coordinate system {}", cs_);
        return;
    }

    // double measured_distance;
    roadmanager::PositionDiff pos_diff;
    bool                      path_found = pos2.Delta(&pos1, pos_diff);  // pos1 is the actor, pos2 is the referenced object

    if (!path_found)
    {
        LOG_ERROR("LateralDistanceAction: No path found between objects {} and {}", object_->GetId(), target_object_->GetId());
        OSCAction::End();
        return;
    }

    bool facing_forward = IsAngleForward(object_->pos_.GetHRelative());
    distance_error      = 0.0;

    // Both cars facing same direction, either along the driving direction or against it
    if ((facing_forward && pos_diff.dDirection) || (!facing_forward && !pos_diff.dDirection))
    {
        distance_error = -(pos_diff.dt + distance_);
    }
    else  // The cars are facing opposite directions
    {
        distance_error = -(pos_diff.dt - distance_);
    }
}

void LatDistanceAction::Step(double simTime, double dt)
{
    (void)simTime;
    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomains::DOMAIN_LAT)))
    {
        // lateral motion controlled elsewhere
        return;
    }

    if (cs_ == roadmanager::CoordinateSystem::CS_ENTITY)
    {
        // find intersection of object's y axis and target object's x-axis
        double target_obj_x_axis[2] = {0.0, 0.0};
        double target_obj_offset[2] = {0.0, 0.0};
        double target_pos[2]        = {0.0, 0.0};
        double step_len             = object_->GetSpeed() * dt;

        RotateVec2D(1.0, 0.0, target_object_->pos_.GetH(), target_obj_x_axis[0], target_obj_x_axis[1]);
        RotateVec2D(0.0, distance_, target_object_->pos_.GetH(), target_obj_offset[0], target_obj_offset[1]);

        // calculate target position, i.e. aiming point
        ProjectPointOnLine2D(object_->pos_.GetX(),
                             object_->pos_.GetY(),
                             target_object_->pos_.GetX() + target_obj_offset[0],
                             target_object_->pos_.GetY() + target_obj_offset[1],
                             target_object_->pos_.GetX() + target_obj_offset[0] + target_obj_x_axis[0],
                             target_object_->pos_.GetY() + target_obj_offset[1] + target_obj_x_axis[1],
                             target_pos[0],
                             target_pos[1]);

        double delta[2] = {target_pos[0] - object_->pos_.GetX(), target_pos[1] - object_->pos_.GetY()};
        double dist     = GetLengthOfVector2D(delta[0], delta[1]);

        if (dynamics_.max_acceleration_ >= LARGE_NUMBER)
        {
            // move instantly to target position
            if (dist < fabs(object_->GetSpeed()) * dt)
            {
                // given speed, some distance remains for moving forward
                double dist_remains = fabs(object_->GetSpeed()) * dt - dist;
                object_->pos_.SetInertiaPos(target_pos[0] + target_obj_x_axis[0] * dist_remains,
                                            target_pos[1] + target_obj_x_axis[1] * dist_remains,
                                            target_object_->pos_.GetH());
            }
            else
            {
                object_->pos_.SetInertiaPos(target_pos[0], target_pos[1], target_object_->pos_.GetH());
            }
        }
        else
        {
            double delta_norm[2] = {0.0, 0.0};
            NormalizeVec2D(delta[0], delta[1], delta_norm[0], delta_norm[1]);
            double new_pos[2] = {0.0, 0.0};
            if (dist < SMALL_NUMBER)
            {
                // on parallel line at specified distance, just move along
                RotateVec2D(1.0, 0.0, target_object_->pos_.GetH(), new_pos[0], new_pos[1]);
                object_->pos_.SetInertiaPos(object_->pos_.GetX() + new_pos[0] * step_len,
                                            object_->pos_.GetY() + new_pos[1] * step_len,
                                            target_object_->pos_.GetH());
            }
            else
            {
                // calculate turn radius based on acceleration constraint, clamp 1000 m/s^2 for nuneric stability
                // acceleration peaking at mid curve, approximate 1.5 times with a pure circular path
                double turn_radius = MAX(0.5, 1.5 * pow(object_->GetSpeed(), 2) / MIN(fabs(dynamics_.max_acceleration_), 1000));
                double dh_max      = MIN(fabs((object_->GetSpeed() / turn_radius) * dt) * 1.5, M_PI - SMALL_NUMBER);

                if (dist < step_len / 2 - SMALL_NUMBER && step_len / 2 > turn_radius - SMALL_NUMBER)
                {
                    // if distance to target point is small, move directly to the target
                    double i0[2], i1[2];
                    GetIntersectionsOfLineAndCircle(
                        {target_object_->pos_.GetX() + target_obj_offset[0], target_object_->pos_.GetY() + target_obj_offset[1]},
                        {target_object_->pos_.GetX() + target_obj_offset[0] + target_obj_x_axis[0],
                         target_object_->pos_.GetY() + target_obj_offset[1] + target_obj_x_axis[1]},
                        {object_->pos_.GetX(), object_->pos_.GetY()},
                        step_len,
                        i0,
                        i1);

                    // pick point most aligned with current heading
                    double h0 = GetAngleOfVector(i0[0] - object_->pos_.GetX(), i0[1] - object_->pos_.GetY());
                    double h1 = GetAngleOfVector(i1[0] - object_->pos_.GetX(), i1[1] - object_->pos_.GetY());
                    if (GetAbsAngleDifference(h0, target_object_->pos_.GetH()) < GetAbsAngleDifference(h1, target_object_->pos_.GetH()))
                    {
                        // pick first intersection
                        object_->pos_.SetInertiaPos(i0[0], i0[1], h0);
                    }
                    else
                    {
                        // pick second intersection
                        object_->pos_.SetInertiaPos(i1[0], i1[1], h1);
                    }
                }
                else
                {
                    // move towards the target distance line
                    double dh = 0.0;
                    if (dist > turn_radius - SMALL_NUMBER)
                    {
                        // if distance to target point is large, move straight towards the target while limit heading change rate
                        double target_heading = atan2(target_pos[1] - object_->pos_.GetY(), target_pos[0] - object_->pos_.GetX());
                        dh                    = ABS_LIMIT(GetAngleDifference(target_heading, object_->pos_.GetH()), dh_max);
                    }
                    else
                    {
                        // within turning radius, align heading with target vehicle while moving towards the target distance line
                        // calculate new driving direction (heading) based on distance and heading alignment with target line
                        double h_diff_delta = GetAngleDifference(GetAngleOfVector(delta[0], delta[1]), object_->pos_.GetH());
                        double h_diff_heading =
                            GetAngleDifference(GetAngleOfVector(target_obj_x_axis[0], target_obj_x_axis[1]), object_->pos_.GetH());

                        double d_angle = step_len / turn_radius;  // angle in radians repersenting the distance along the segment

                        // calculate linear transition parameter w based on mean tangent angle on the segment
                        double angle = MAX(acos(1 - dist / turn_radius), d_angle / 2);
                        double w     = (2 * angle - d_angle / 2) / M_PI;

                        // then apply cubic smoothing for C1 continuity, if acc_y set
                        w = 3 * pow(w, 2) - 2 * pow(w, 3);

                        // calculate delta heading based on distance to target point and heading alignment
                        dh = ABS_LIMIT((w)*h_diff_delta + (1 - w) * h_diff_heading, dh_max);
                    }
                    double new_heading = GetAngleInInterval2PI(object_->pos_.GetH() + dh);
                    new_pos[0]         = object_->pos_.GetX() + cos(new_heading) * step_len;
                    new_pos[1]         = object_->pos_.GetY() + sin(new_heading) * step_len;

                    object_->pos_.SetInertiaPos(new_pos[0], new_pos[1], new_heading);
                }
            }
        }
        object_->SetDirtyBits(Object::DirtyBit::LATERAL | Object::DirtyBit::LONGITUDINAL | Object::DirtyBit::SPEED);
    }
    else if (cs_ == roadmanager::CoordinateSystem::CS_ROAD)
    {
        switch (move_state_)
        {
            case MoveState::INIT:
            {
                if (dynamics_.max_speed_ >= LARGE_NUMBER && dynamics_.max_acceleration_ >= LARGE_NUMBER &&
                    dynamics_.max_deceleration_ >= LARGE_NUMBER)
                {
                    move_state_ = MoveState::MOVE_RIGID;
                }
                else
                {
                    spring_.SetTension(0.4 * dynamics_.max_acceleration_);  // Adjust spring tension based on max acceleration
                    move_state_ = MoveState::MOVE_DYNAMIC;
                }
                old_x_ = object_->pos_.GetX();
                old_y_ = object_->pos_.GetY();
            }
            break;
            case MoveState::MOVE_RIGID:
            {
                double distance_error;
                GetDistanceError(object_->pos_, target_object_->pos_, distance_error);

                roadmanager::Position desired_pos;
                GetDesiredRoadPos(distance_error, desired_pos);
                object_->pos_.SetLanePos(desired_pos.GetTrackId(), desired_pos.GetLaneId(), desired_pos.GetS(), desired_pos.GetOffset());
                object_->SetSpeed(object_->GetSpeed());
                if (!continuous_)
                {
                    object_->pos_.SetHeading(atan2(desired_pos.GetY() - old_y_, desired_pos.GetX() - old_x_));

                    if (std::abs(distance_error) < LATERAL_DISTANCE_THRESHOLD)
                    {
                        // Reached requested distance, quit action
                        move_state_ = MoveState::INIT;
                        OSCAction::End();
                    }
                }
                else
                {
                    object_->pos_.SetHeading(atan2(desired_pos.GetY() - old_y_, desired_pos.GetX() - old_x_));
                }
                old_x_ = object_->pos_.GetX();
                old_y_ = object_->pos_.GetY();
                object_->SetDirtyBits(Object::DirtyBit::LATERAL);
            }
            break;
            case (MoveState::MOVE_DYNAMIC):
            {
                // Find desired position
                double distance_error;
                GetDistanceError(object_->pos_, target_object_->pos_, distance_error);

                if (LARGE_NUMBER != dynamics_.max_acceleration_)
                {
                    // Parameters
                    // For the spring values x and x0, we set the current distance error and target value to 0.0 since we have already calculated the
                    // distance error. Distance error is negated to ensure that the spring force acts in the correct direction
                    spring_.SetValue(-distance_error);
                    spring_.SetV(lat_vel_ - target_object_->pos_.GetVelLat());  // Speed difference in lateral direction
                    spring_.Update(dt);

                    // Clamp the change in acceleration (delta_accel) by jerk limits
                    double delta_accel =
                        CLAMP(spring_.GetA() - acceleration_, -(dynamics_.max_acceleration_rate_ * dt), dynamics_.max_acceleration_rate_ * dt);

                    // Calculate the new acceleration after applying jerk limits
                    acceleration_ = CLAMP(acceleration_ + delta_accel, -dynamics_.max_acceleration_, dynamics_.max_acceleration_);

                    // Subtract small number to maintain some longitudinal velocity, avoid driving in wrong direction if lat speed is capped
                    lat_vel_ = ABS_LIMIT(lat_vel_ + acceleration_ * dt, (std::min(dynamics_.max_speed_, object_->GetSpeed()) - SMALL_NUMBER));

                    if (!continuous_ && std::abs(distance_error) < LATERAL_DISTANCE_THRESHOLD)
                    {
                        // Reached requested distance, quit action
                        move_state_ = MoveState::INIT;
                        OSCAction::End();
                    }
                }
                else
                {
                    // Subtract small number to maintain some longitudinal velocity, avoid driving in wrong direction if lat speed is capped
                    lat_vel_ = SIGN(distance_error) * (std::min(dynamics_.max_speed_, object_->GetSpeed()) - SMALL_NUMBER);

                    double d_offset  = lat_vel_ * dt;
                    double new_error = distance_error - d_offset;
                    if (SIGN(new_error) != SIGN(distance_error))
                    {
                        d_offset = distance_error;
                        lat_vel_ = d_offset / dt;

                        if (!continuous_)
                        {
                            OSCAction::End();
                        }
                    }
                }

                double long_vel = sqrt(pow(object_->GetSpeed(), 2) - pow(lat_vel_, 2));

                object_->MoveAlongS(long_vel * dt);
                object_->pos_.SetLanePos(object_->pos_.GetTrackId(),
                                         object_->pos_.GetLaneId(),
                                         object_->pos_.GetS(),
                                         object_->pos_.GetOffset() + lat_vel_ * dt);

                object_->pos_.SetHeading(atan2(object_->pos_.GetY() - old_y_, object_->pos_.GetX() - old_x_));

                object_->SetSpeed(object_->GetSpeed());
                object_->SetDirtyBits(Object::DirtyBit::LONGITUDINAL | Object::DirtyBit::LATERAL | Object::DirtyBit::SPEED);

                old_x_ = object_->pos_.GetX();
                old_y_ = object_->pos_.GetY();
            }
        }
    }
}

void LatDistanceAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (target_object_ == obj1)
    {
        target_object_ = obj2;
    }
}

void TeleportAction::Start(double simTime)
{
    OSCAction::Start(simTime);
    LOG_INFO("Starting teleport Action");

    if (object_->IsGhost() && IsGhostRestart() && scenarioEngine_->getSimulationTime() > SMALL_NUMBER)
    {
        SE_Env::Inst().SetGhostMode(GhostMode::RESTART);

        object_->trail_.Reset(true);
        object_->trail_.SetInterpolationMode(roadmanager::PolyLineBase::InterpolationMode::INTERPOLATE_SEGMENT);

        // The following code will copy speed from the Ego that ghost relates to
        if (object_->ghost_Ego_ != nullptr)
        {
            object_->SetSpeed(object_->ghost_Ego_->GetSpeed());
        }

        scenarioEngine_->ResetEvents();  // Ghost-project. Reset events finished by ghost.
    }

    if (object_->IsControllerModeOnAnyOfDomains(ControlOperationMode::MODE_OVERRIDE,
                                                static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT_AND_LONG)))
    {
        // motion controlled elsewhere
        return;
    }

    if (object_->TowVehicle())
    {
        return;  // position controlled by tow vehicle
    }

    // layout trajectory if defined
    if (position_.GetTrajectory() != nullptr)
    {
        position_.GetTrajectory()->Freeze(FollowingMode::POSITION, 0);
    }

    // consider any assigned route for relative positions
    position_.CopyRoute(object_->pos_);
    object_->pos_.TeleportTo(&position_);

    if (!object_->TowVehicle() && object_->TrailerVehicle())
    {
        (static_cast<Vehicle*>(object_))->AlignTrailers();
    }

    LOG_INFO("{} New position:", object_->name_);
    object_->pos_.Print();

    object_->SetDirtyBits(Object::DirtyBit::LATERAL | Object::DirtyBit::LONGITUDINAL | Object::DirtyBit::SPEED | Object::DirtyBit::TELEPORT);
    object_->reset_ = true;
}

void TeleportAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

void TeleportAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    position_.ReplaceObjectRefs(&obj1->pos_, &obj2->pos_);
}

void ConnectTrailerAction::Start(double simTime)
{
    OSCAction::Start(simTime);

    if (trailer_object_)
    {
        if (object_->TrailerVehicle())
        {
            if (object_->TrailerVehicle() == trailer_object_)
            {
                LOG_INFO("Trailer {} already connected to {} - keep connection", trailer_object_->GetName(), object_->GetName());
            }
            else
            {
                LOG_INFO("Disconnecting currently connected trailer: {}", object_->TrailerVehicle()->GetName());
                reinterpret_cast<Vehicle*>(object_)->DisconnectTrailer();
            }
        }

        if (trailer_object_ != object_->TrailerVehicle())
        {
            LOG_INFO("Connect trailer {} to {}", reinterpret_cast<Vehicle*>(trailer_object_)->GetName(), object_->GetName());
            reinterpret_cast<Vehicle*>(object_)->ConnectTrailer(reinterpret_cast<Vehicle*>(trailer_object_));
        }
    }
    else
    {
        LOG_INFO("No trailer to disconnect from {}", object_->GetName());
    }
}

void ConnectTrailerAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

void ConnectTrailerAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }

    if (trailer_object_ == obj1)
    {
        trailer_object_ = obj2;
    }
}

void DisconnectTrailerAction::Start(double simTime)
{
    OSCAction::Start(simTime);

    if (object_->TrailerVehicle())
    {
        LOG_INFO("Disconnecting {} from {}", object_->TrailerVehicle()->GetName(), object_->GetName());
        reinterpret_cast<Vehicle*>(object_)->DisconnectTrailer();
    }
    else
    {
        LOG_WARN("DisconnectTrailerAction: No trailer connected, ignoring action");
    }
}

void DisconnectTrailerAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

void DisconnectTrailerAction::ReplaceObjectRefs(Object* obj1, Object* obj2)
{
    if (object_ == obj1)
    {
        object_ = obj2;
    }
}

double SynchronizeAction::CalcSpeedForLinearProfile(double v_final, double time, double dist)
{
    if (time < 0.001 || dist < 0.001)
    {
        // Avoid division by zero, fall back to zero acceleration
        return 0;
    }

    // Compute current speed needed to reach given final speed in given time
    double v0 = 2 * dist / time - v_final;

    return v0;
}

const char* SynchronizeAction::Mode2Str(SynchMode mode) const
{
    if (mode == SynchMode::MODE_NONE)
    {
        return "MODE_NONE";
    }
    else if (mode == SynchMode::MODE_NON_LINEAR)
    {
        return "MODE_NON_LINEAR";
    }
    else if (mode == SynchMode::MODE_LINEAR)
    {
        return "MODE_LINEAR";
    }
    else if (mode == SynchMode::MODE_STOPPED)
    {
        return "MODE_STOPPED";
    }
    else if (mode == SynchMode::MODE_STOP_IMMEDIATELY)
    {
        return "MODE_STOP_IMMEDIATELY";
    }
    else if (mode == SynchMode::MODE_WAITING)
    {
        return "MODE_WAITING";
    }
    else if (mode == SynchMode::MODE_STEADY_STATE)
    {
        return "MODE_STEADY_STATE";
    }
    else
    {
        return "Unknown mode";
    }
}

const char* SynchronizeAction::SubMode2Str(SynchSubmode submode) const
{
    if (submode == SynchSubmode::SUBMODE_CONCAVE)
    {
        return "SUBMODE_CONCAVE";
    }
    else if (submode == SynchSubmode::SUBMODE_CONVEX)
    {
        return "SUBMODE_CONVEX";
    }
    else if (submode == SynchSubmode::SUBMODE_NONE)
    {
        return "SUBMODE_NONE";
    }
    else
    {
        return "Unknown sub-mode";
    }
}

void SynchronizeAction::PrintStatus(const char* custom_msg)
{
    LOG_DEBUG("Synchronize: {}, mode={} ({}) sub-mode={} ({})", custom_msg, Mode2Str(mode_), mode_, SubMode2Str(submode_), submode_);
}

void SynchronizeAction::SetMode(SynchMode mode, std::string msg)
{
    if (mode != mode_)
    {
        mode_ = mode;
        PrintStatus(("SetMode " + msg).c_str());
    }
}

void SynchronizeAction::SetSubMode(SynchSubmode submode, std::string msg)
{
    if (submode != submode_)
    {
        submode_ = submode;
        PrintStatus(("SetSubMode" + msg).c_str());
    }
}

void SynchronizeAction::Start(double simTime)
{
    target_position_master_.EvaluateRelation();
    target_position_.EvaluateRelation();

    // resolve steady state -> translate into dist
    if (steadyState_.type_ == SteadyStateType::STEADY_STATE_TIME)
    {
        steadyState_.dist_ = steadyState_.time_ * final_speed_->GetValue();
        steadyState_.type_ = SteadyStateType::STEADY_STATE_DIST;
    }
    else if (steadyState_.type_ == SteadyStateType::STEADY_STATE_POS)
    {
        // Find out distance between steady state position and final destination
        roadmanager::PositionDiff diff;
        target_position_.Delta(&steadyState_.pos_, diff);
        steadyState_.dist_ = diff.ds;
        steadyState_.type_ = SteadyStateType::STEADY_STATE_DIST;
    }

    OSCAction::Start(simTime);

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG)))
    {
        // longitudinal motion controlled elsewhere
        return;
    }
}

SynchronizeAction::~SynchronizeAction()
{
    if (target_position_master_.GetTrajectory() != nullptr)
    {
        delete target_position_master_.GetTrajectory();
    }

    if (target_position_.GetTrajectory() != nullptr)
    {
        delete target_position_.GetTrajectory();
    }

    if (steadyState_.type_ == SteadyStateType::STEADY_STATE_POS && steadyState_.pos_.GetTrajectory() != nullptr)
    {
        delete steadyState_.pos_.GetTrajectory();
    }
}

void SynchronizeAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;
    bool done                      = false;
    int  distance_direction_master = 1;  // assume by default that destination is ahead

    if (object_->IsControllerModeOnDomains(ControlOperationMode::MODE_OVERRIDE, static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG)))
    {
        // longitudinal motion controlled elsewhere
        return;
    }

    // Calculate distance along road/route
    double                    masterDist = LARGE_NUMBER;
    double                    dist       = LARGE_NUMBER;
    roadmanager::PositionDiff diff;

    int retval = -1;
    if (master_object_->pos_.GetTrajectory())
    {
        retval = master_object_->pos_.Distance(&target_position_master_,
                                               roadmanager::CoordinateSystem::CS_TRAJECTORY,
                                               roadmanager::RelativeDistanceType::REL_DIST_LONGITUDINAL,
                                               masterDist);
        distance_direction_master =
            SIGN(masterDist) *
            (IsAngleForward(GetAngleDifference(master_object_->pos_.GetTrajectory()->GetHTrue(), master_object_->pos_.GetH())) ? 1 : -1);
        masterDist = fabs(masterDist);
    }

    if (retval == -1 || masterDist > LARGE_NUMBER - SMALL_NUMBER)
    {
        if (!master_object_->pos_.Delta(&target_position_master_, diff))
        {
            // No road network path between master vehicle and master target pos - using world coordinate distance
            double distance_vector[2]     = {target_position_master_.GetX() - master_object_->pos_.GetX(),
                                             target_position_master_.GetY() - master_object_->pos_.GetY()};
            diff.ds                       = GetLengthOfVector2D(distance_vector[0], distance_vector[1]);
            masterDist                    = fabs(diff.ds);
            double heading_to_destination = GetAngleOfVector(distance_vector[0], distance_vector[1]);
            distance_direction_master     = IsAngleForward(GetAngleDifference(heading_to_destination, master_object_->pos_.GetH())) ? 1 : -1;
        }
        else
        {
            distance_direction_master = SIGN(diff.ds);
            masterDist                = fabs(diff.ds);
        }
    }

    retval = -1;
    if (object_->pos_.GetTrajectory())
    {
        retval = object_->pos_.Distance(&target_position_,
                                        roadmanager::CoordinateSystem::CS_TRAJECTORY,
                                        roadmanager::RelativeDistanceType::REL_DIST_LONGITUDINAL,
                                        dist);
        dist   = fabs(dist);
    }

    if (retval == -1 || dist > LARGE_NUMBER - SMALL_NUMBER)
    {
        if (!object_->pos_.Delta(&target_position_, diff))
        {
            // No road network path between action vehicle and action target pos - using world coordinate distance
            double distance_vector[2] = {target_position_.GetX() - object_->pos_.GetX(), target_position_.GetY() - object_->pos_.GetY()};
            diff.ds                   = GetLengthOfVector2D(distance_vector[0], distance_vector[1]);
            dist                      = fabs(diff.ds);
        }
        else
        {
            dist = fabs(diff.ds);
        }
    }

    // Done when distance increases, indicating that destination just has been reached or passed
    if (dist < tolerance_ + SMALL_NUMBER)
    {
        LOG_INFO("Synchronize dist ({:.2f}) < tolerance ({:.2f})", dist, tolerance_);
        done = true;
    }
    else if (masterDist < tolerance_master_ + SMALL_NUMBER)
    {
        LOG_INFO("Synchronize masterDist ({:.2f}) < tolerance ({:.2f})", masterDist, tolerance_master_);
        done = true;
    }
    else if (dist > lastDist_)
    {
        LOG_INFO("Synchronize dist increasing ({:.2f} > {:.2f}) - missed destination", dist, lastDist_);
        done = true;
    }
    else if (masterDist > lastMasterDist_)
    {
        LOG_INFO("Synchronize masterDist increasing ({:.2f} > {:.2f}) - missed destination", masterDist, lastMasterDist_);
        done = true;
    }

    lastDist_       = dist;
    lastMasterDist_ = masterDist;

    // for calculations, measure distance to toleration area/radius
    dist       = MAX(dist - tolerance_, SMALL_NUMBER);
    masterDist = MAX(masterDist - tolerance_master_, SMALL_NUMBER);

    double masterTimeToDest = LARGE_NUMBER;

    // project speed on road s-axis
    double master_speed = SIGN(master_object_->GetSpeed()) * distance_direction_master * fabs(master_object_->pos_.GetVelS());

    if (master_speed > SMALL_NUMBER)
    {
        masterTimeToDest = masterDist / master_speed;

        if (masterTimeToDest < dt)
        {
            LOG_INFO("Synchronize masterTimeToDest ({:.3f}) reached within this timestep ({:.3f})", masterTimeToDest, dt);
            done = true;
        }
    }
    else if (mode_ != SynchMode::MODE_WAITING)
    {
        SetMode(SynchMode::MODE_WAITING, "Waiting for master to move");
        // Master is standing still, do not move
        object_->SetSpeed(0);
    }

    if (done)
    {
        if (final_speed_)
        {
            object_->SetSpeed(final_speed_->GetValue());
        }
        OSCAction::End();
    }
    else
    {
        double average_speed = dist / masterTimeToDest;
        double acc           = 0;

        if (final_speed_)
        {
            if (steadyState_.type_ != SteadyStateType::STEADY_STATE_NONE && mode_ != SynchMode::MODE_STEADY_STATE)
            {
                double time_to_ss = steadyState_.dist_ / final_speed_->GetValue();

                if (dist - steadyState_.dist_ < SMALL_NUMBER || masterTimeToDest - time_to_ss < SMALL_NUMBER)
                {
                    SetMode(SynchMode::MODE_STEADY_STATE);
                    SetSubMode(SynchSubmode::SUBMODE_NONE);
                    if (time_to_ss > masterTimeToDest && (time_to_ss - masterTimeToDest) * final_speed_->GetValue() > tolerance_)
                    {
                        LOG_INFO("Entering Stead State according to criteria but not enough time to reach destination");
                    }
                }
                else
                {
                    // subtract steady state distance
                    dist -= steadyState_.dist_;

                    // subtract steady state duration
                    masterTimeToDest -= time_to_ss;
                }
            }

            // For more information about calculations, see
            // https://docs.google.com/document/d/1dEBUWlJVLUz6Rp9Ol1l90iG0LfNtcsgLyJ0kDdwgPzA/edit?usp=sharing
            //
            // Interactive Python script plotting calculation result based on various input values
            // https://drive.google.com/file/d/1z902gRYogkLhUAV1pZLc9gcgwnak7TBH/view?usp=sharing
            // (the method described below is "Spedified final speed - alt 1")
            //
            // Here follow a brief description:
            //
            // Calculate acceleration needed to reach the destination in due time
            // Four cases
            //   1  Linear. Reach final speed with constant acceleration
            //	 2a Non-linear convex (rush). First accelerate, then decelerate.
            //	 2b Non-linear concave (linger). First decelerate, then accelerate.
            //   3  Non-linear with stop. Decelerate to a stop. Wait. Then accelerate.
            //
            //   Case 2-3 involves two (case 2a, 2b) or three (case 3) phases with constant acceleration/deceleration
            //   Last phase in case 2-3 is actually case 1 - a linear change to final speed
            //
            // Symbols
            //   given:
            //     s = distance to destination
            //     t = master object time to destination
            //     v0 = current speed
            //     v1 = final speed
            //     va = Average speed needed to reach destination
            //   variables:
            //     s1 = distance first phase
            //     s2 = distance second phase
            //     x = end time for first phase
            //     y = end time for second (last) phase
            //     vx = speed at x
            //
            // Equations
            //   case 1
            //     v1 = 2 * s / t - v2
            //     a = (v2 - v1) / t
            //
            //   case 2 (a & b)
            //      system:
            //        s1 = v1 * x + (vx - v1) * x / 2
            //        s2 = v2 * y + (vx - v2) * y / 2
            //        t = x + y
            //        s = s1 + s2
            // 		  (vx - v1) / x = (vx - v2) / y
            //
            //      solve for x and vx
            //      a = (vx - v1) / x
            //
            //   case 3
            //      system:
            //        s1 = x * v1 / 2
            //        s2 = y * v2 / 2
            //        s = s1 + s2
            //        v1 / v2 = x / y
            //
            //      solve for x
            //      a = -v1 / x

            if (mode_ == SynchMode::MODE_STEADY_STATE)
            {
                object_->speed_ = final_speed_->GetValue();
                return;
            }
            if (mode_ == SynchMode::MODE_WAITING)
            {
                if (masterTimeToDest >= LARGE_NUMBER)
                {
                    // Continue waiting
                    return;
                }
                else
                {
                    // Reset mode
                    SetMode(SynchMode::MODE_NONE, "Reset");
                }
            }
            if (mode_ == SynchMode::MODE_STOP_IMMEDIATELY)
            {
                acc = MAX_DECELERATION;
                object_->speed_ += acc * dt;
                if (object_->speed_ < 0)
                {
                    object_->SetSpeed(0);
                    SetMode(SynchMode::MODE_WAITING, "Waiting");
                }

                return;
            }
            else if (mode_ == SynchMode::MODE_STOPPED)
            {
                if (masterTimeToDest < 2 * dist / final_speed_->GetValue())
                {
                    // Time to move again after the stop
                    SetMode(SynchMode::MODE_LINEAR, "Restart");
                }
                else
                {
                    // Stay still
                    object_->SetSpeed(0);
                    return;
                }
            }

            if (mode_ == SynchMode::MODE_LINEAR)
            {
                if (masterTimeToDest > LARGE_NUMBER - 1)
                {
                    // Master in effect standing still, do not move
                    object_->SetSpeed(0);
                }
                else
                {
                    object_->SetSpeed(MAX(0, CalcSpeedForLinearProfile(MAX(0, final_speed_->GetValue()), masterTimeToDest, dist)));
                }
                return;
            }
            else if (mode_ == SynchMode::MODE_NON_LINEAR && masterTimeToDest < LARGE_NUMBER)
            {
                // Check if case 1, i.e. on a straight speed profile line
                double v0_onLine = 2 * dist / masterTimeToDest - final_speed_->GetValue();

                if (fabs(object_->speed_ - v0_onLine) < 0.1)
                {
                    // Passed apex. Switch to linear mode (constant acc) to reach final destination and speed
                    SetMode(SynchMode::MODE_LINEAR, "Passed apex");

                    // Keep current speed for this time step
                    return;
                }
            }

            if (mode_ == SynchMode::MODE_NONE)
            {
                // Since not linear mode, go into non-linear mode
                SetMode(SynchMode::MODE_NON_LINEAR);

                // Find out submode, convex or concave
                if ((final_speed_->GetValue() + object_->speed_) / 2 < dist / masterTimeToDest)
                {
                    SetSubMode(SynchSubmode::SUBMODE_CONVEX);
                }
                else
                {
                    SetSubMode(SynchSubmode::SUBMODE_CONCAVE);
                }
            }

            // Now, calculate x and vx according to default method oulined in the documentation
            double s = dist;
            double t = masterTimeToDest;

            // project speed on road s-axis
            double v0 = SIGN(object_->GetSpeed()) * abs(object_->pos_.GetVelS());
            double v1 = final_speed_->GetValue();

            // Calculate both solutions from the quadratic equation
            double vx = 0;
            if (fabs(v1 - v0) < SMALL_NUMBER)
            {
                // When v0 == v1, x is simply t/2
                // s = (T / 2) * v_cur + (T / 2) * vx -> vx = (s - (T / 2) * v_cur) / (T / 2)
                vx  = (s - (t / 2) * v0) / (t / 2);
                acc = (vx - v0) / (t / 2);
            }
            else
            {
                double signed_term = sqrt(2.0) * sqrt(2.0 * s * s - 2 * (v1 + v0) * t * s + (v1 * v1 + v0 * v0) * t * t);
                double x1          = -(signed_term + 2 * s - 2 * v1 * t) / (2 * (v1 - v0));
                double x2          = -(-signed_term + 2 * s - 2 * v1 * t) / (2 * (v1 - v0));

                // Choose solution, only one is found within the given time span [0:masterTimeToDest]
                if (x1 > 0 && x1 < t)
                {
                    double vx1 = (2 * s - signed_term) / (2 * t);
                    double a1  = (vx1 - v0) / x1;
                    vx         = vx1;
                    acc        = a1;
                }
                else if (x2 > 0 && x2 < t)
                {
                    double vx2 = (2 * s + signed_term) / (2 * t);
                    double a2  = (vx2 - v0) / x2;
                    vx         = vx2;
                    acc        = a2;
                }
                else
                {
                    // No solution
                    acc = 0;
                }
            }

            if (mode_ == SynchMode::MODE_NON_LINEAR &&
                ((submode_ == SynchSubmode::SUBMODE_CONCAVE && acc > 0) || (submode_ == SynchSubmode::SUBMODE_CONVEX && acc < 0)))
            {
                // Reached the apex of the speed profile, switch mode and phase
                SetMode(SynchMode::MODE_LINEAR, "Reached apex");

                // Keep speed for this time step
                acc = 0;
            }

            // Check for case 3, where target speed(vx) < 0
            if (mode_ == SynchMode::MODE_NON_LINEAR && vx < 0)
            {
                // In phase one, decelerate to 0, then stop
                // Calculate time needed to cover distance proportional to current speed / final speed
                double t1 = 2 * v0 * s / (v0 * v0 + v1 * v1);
                if (fabs(t1) > SMALL_NUMBER)
                {
                    acc = -v0 / t1;
                }
                else
                {
                    acc = 0;
                }

                if (t1 * v0 / 2 > s / 2)
                {
                    // If more than half distance to destination needed, then stop immediatelly
                    acc = MAX_DECELERATION;
                    SetMode(SynchMode::MODE_STOP_IMMEDIATELY);
                }

                if (v0 + acc * dt < 0)
                {
                    // Reached to a stop
                    object_->SetSpeed(0);
                    SetMode(SynchMode::MODE_STOPPED);

                    return;
                }
            }
        }
        else
        {
            // No final speed specified. Calculate it based on current speed and available time
            if (master_speed > SMALL_NUMBER)
            {
                if (mode_ != SynchMode::MODE_LINEAR)
                {
                    SetMode(SynchMode::MODE_LINEAR, "Moving towards destination");
                }

                double final_speed = 2 * average_speed - object_->speed_;
                acc                = (final_speed - object_->speed_) / masterTimeToDest;
            }
        }

        object_->SetSpeed(object_->GetSpeed() + acc * dt);
    }
}

void VisibilityAction::Start(double simTime)
{
    OSCAction::Start(simTime);
    object_->SetVisibilityMask((graphics_ ? Object::Visibility::GRAPHICS : 0) | (traffic_ ? Object::Visibility::TRAFFIC : 0) |
                               (sensors_ ? Object::Visibility::SENSORS : 0));
    auto towVehicle = object_->TowVehicle();
    if (towVehicle)
    {
        towVehicle->SetVisibilityMask((graphics_ ? Object::Visibility::GRAPHICS : 0) | (traffic_ ? Object::Visibility::TRAFFIC : 0) |
                                      (sensors_ ? Object::Visibility::SENSORS : 0));
    }
    auto TrailerVehicle = object_->TrailerVehicle();
    if (TrailerVehicle)
    {
        TrailerVehicle->SetVisibilityMask((graphics_ ? Object::Visibility::GRAPHICS : 0) | (traffic_ ? Object::Visibility::TRAFFIC : 0) |
                                          (sensors_ ? Object::Visibility::SENSORS : 0));
    }
}

void VisibilityAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

int OverrideControlAction::AddOverrideStatus(Object::OverrideActionStatus status)
{
    overrideActionList.push_back(status);
    if (status.type < Object::OverrideType::OVERRIDE_NR_TYPES)
    {
        if (status.type == Object::OverrideType::OVERRIDE_STEERING_WHEEL)
        {
            if (status.active)
            {
                domains_ = domains_ | static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT);
            }
            else
            {
                domains_ = domains_ & ~static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LAT);
            }
        }
        else
        {
            if (status.active)
            {
                domains_ = domains_ | static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG);
            }
            else
            {
                domains_ = domains_ & ~static_cast<unsigned int>(ControlDomainMasks::DOMAIN_MASK_LONG);
            }
        }
    }
    else
    {
        LOG_ERROR_AND_QUIT("Unexpected override type: {}", status.type);
    }

    return 0;
}

void OverrideControlAction::Start(double simTime)
{
    for (size_t i = 0; i < overrideActionList.size(); i++)
    {
        object_->overrideActionList[overrideActionList[i].type] = overrideActionList[i];
    }
    OSCAction::Start(simTime);
}

void OverrideControlAction::Step(double simTime, double dt)
{
    (void)simTime;
    (void)dt;

    OSCAction::End();
}

double OverrideControlAction::RangeCheckAndErrorLog(Object::OverrideType type, double valueCheck, double lowerLimit, double upperLimit, bool ifRound)
{
    double temp = valueCheck;
    if (valueCheck <= upperLimit && valueCheck >= lowerLimit)
    {
        if (!ifRound)
        {
            LOG_INFO("{} value {:.2f} is within range.", type, valueCheck);
        }
        else
        {
            valueCheck = round(temp);
            LOG_INFO("{} value {:.1f} is within range and the value is rounded to {:.1f}.", type, temp, valueCheck);
        }
    }
    else
    {
        valueCheck = (valueCheck > upperLimit) ? upperLimit : lowerLimit;
        LOG_INFO("{} value is not within range and is modified from {:.1f} to {:.1f}.", type, temp, valueCheck);
    }
    return valueCheck;
}
