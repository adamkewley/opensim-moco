/* -------------------------------------------------------------------------- *
 * OpenSim Moco: examplePredictive.cpp                                        *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Antoine Falisse                                                 *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0          *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include <OpenSim/Simulation/SimbodyEngine/PinJoint.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <OpenSim/Common/LinearFunction.h>
#include <Moco/osimMoco.h>
#include <Moco/Components/SmoothSphereHalfSpaceForce.h>

using namespace OpenSim;

// This class defines a MocoCost that computes the average speed defined as the
// distance travelled by the pelvis in the forward direction divided by the
// final time
class MocoAverageSpeedCost : public MocoCost {
OpenSim_DECLARE_CONCRETE_OBJECT(MocoAverageSpeedCost, MocoCost);
public:
    OpenSim_DECLARE_PROPERTY(desired_speed, double,
            "The desired forward speed defined as the distance travelled by"
            "the pelvis in the forward direction divided by the final time.");
    MocoAverageSpeedCost() {
        constructProperties();
    }
    MocoAverageSpeedCost(std::string name) : MocoCost(std::move(name)) {
        constructProperties();
    }
    MocoAverageSpeedCost(std::string name, double weight)
            : MocoCost(std::move(name), weight) {
        constructProperties();
    }
protected:
    void calcEndpointCostImpl(const SimTK::State& finalState,
            SimTK::Real& cost) const override {
        // get final time
        SimTK::Real time = finalState.getTime();
        // get final pelvis forward position
        SimTK::Real position =  m_coord->getValue(finalState);
        cost = SimTK::square(get_desired_speed() - (position / time));
    }
    void initializeOnModelImpl(const Model& model) const {
        m_coord.reset(&model.getCoordinateSet().get("groundPelvis_q_tx"));
    }
private:
    void constructProperties() {
        constructProperty_desired_speed(0.0);
    }
    mutable SimTK::ReferencePtr<const Coordinate> m_coord;
};

/// This model is torque-actuated.
std::unique_ptr<Model> createGait2D() {
    /*auto model = make_unique<Model>("gait_2D.osim");*/
    auto model = make_unique<Model>();
    model->setName("gait_2D");

using SimTK::Vec3;
using SimTK::Inertia;
using SimTK::Transform;

// Create model
///////////////////////////////////////////////////////////////////////////
// Add bodies
///////////////////////////////////////////////////////////////////////////
auto* pelvis = new OpenSim::Body("pelvis", 9.7143336091724,
    Vec3(-0.0682778,0,0), Inertia(0.0814928846050306,0.0814928846050306,
    0.0445427591530667,0,0,0));
model->addBody(pelvis);
auto* femur_l = new OpenSim::Body("femur_l", 7.67231915023828,
    Vec3(0,-0.170467,0), Inertia(0.111055472890139,0.0291116288158616,
    0.117110028170931,0,0,0));
model->addBody(femur_l);
auto* femur_r = new OpenSim::Body("femur_r", 7.67231915023828,
    Vec3(0,-0.170467,0), Inertia(0.111055472890139,0.0291116288158616,
    0.117110028170931,0,0,0));
model->addBody(femur_r);
auto* tibia_l = new OpenSim::Body("tibia_l", 3.05815503574821,
    Vec3(0,-0.180489,0), Inertia(0.0388526996597354,0.00393152317985418,
    0.0393923204883429,0,0,0));
model->addBody(tibia_l);
auto* tibia_r = new OpenSim::Body("tibia_r", 3.05815503574821,
    Vec3(0,-0.180489,0), Inertia(0.0388526996597354,0.00393152317985418,
    0.0393923204883429,0,0,0));
model->addBody(tibia_r);
auto* talus_l = new OpenSim::Body("talus_l", 0.082485638186061,
    Vec3(0), Inertia(0.000688967700910182,0.000688967700910182,
    0.000688967700910182,0,0,0));
model->addBody(talus_l);
auto* talus_r = new OpenSim::Body("talus_r", 0.082485638186061,
    Vec3(0), Inertia(0.000688967700910182,0.000688967700910182,
    0.000688967700910182,0,0,0));
model->addBody(talus_r);
auto* calcn_l = new OpenSim::Body("calcn_l", 1.03107047732576,
    Vec3(0.0913924,0.0274177,0), Inertia(0.000964554781274254,
    0.00268697403354971,0.00282476757373175,0,0,0));
model->addBody(calcn_l);
auto* calcn_r = new OpenSim::Body("calcn_r", 1.03107047732576,
    Vec3(0.0913924,0.0274177,0), Inertia(0.000964554781274254,
    0.00268697403354971,0.00282476757373175,0,0,0));
model->addBody(calcn_r);
auto* toes_l = new OpenSim::Body("toes_l", 0.178663892311008,
    Vec3(0.0316218,0.00548355,0.0159937), Inertia(6.88967700910182e-005,
    0.000137793540182036,6.88967700910182e-005,0,0,0));
model->addBody(toes_l);
auto* toes_r = new OpenSim::Body("toes_r", 0.178663892311008,
    Vec3(0.0316218,0.00548355,-0.0159937), Inertia(6.88967700910182e-005,
    0.000137793540182036,6.88967700910182e-005,0,0,0));
model->addBody(toes_r);
auto* torso = new OpenSim::Body("torso", 28.240278003209,
    Vec3(-0.0289722,0.309037,0), Inertia(1.14043571182129,
    0.593400919285897,1.14043571182129,0,0,0));
model->addBody(torso);

///////////////////////////////////////////////////////////////////////////
// Add joints
///////////////////////////////////////////////////////////////////////////
// Ground pelvis
auto* groundPelvis = new PlanarJoint("groundPelvis", model->getGround(),
    Vec3(0), Vec3(0), *pelvis, Vec3(0), Vec3(0));
auto& groundPelvis_q_rz =
    groundPelvis->updCoordinate(PlanarJoint::Coord::RotationZ);
groundPelvis_q_rz.setRangeMin(-SimTK::Pi);
groundPelvis_q_rz.setRangeMax(SimTK::Pi);
groundPelvis_q_rz.setName("groundPelvis_q_rz");
auto& groundPelvis_q_tx =
    groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationX);
groundPelvis_q_tx.setRangeMin(-5);
groundPelvis_q_tx.setRangeMax(5);
groundPelvis_q_tx.setName("groundPelvis_q_tx");
auto& groundPelvis_q_ty =
    groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationY);
groundPelvis_q_ty.setRangeMin(-1);
groundPelvis_q_ty.setRangeMax(3);
groundPelvis_q_ty.setName("groundPelvis_q_ty");
model->addJoint(groundPelvis);

// Hip left
/*SpatialTransform st_hip_l;
st_hip_l[2].setCoordinateNames(
    OpenSim::Array<std::string>("hip_flexion_l", 1, 1));
st_hip_l[2].setFunction(new LinearFunction());
st_hip_l[2].setAxis(Vec3(0, 0, 1));
auto* hip_l = new CustomJoint("hip_l", *pelvis, Vec3(-0.0682778001711179,
    -0.0638353973311301,-0.0823306940058688), Vec3(0), *femur_l, Vec3(0),
    Vec3(0), st_hip_l);
model->addJoint(hip_l);
hip_l->finalizeFromProperties();
auto& hip_q_l = hip_l->upd_coordinates(0);
hip_q_l.setName("hip_q_l");*/
auto* hip_l = new PinJoint("hip_l", *pelvis, Vec3(-0.0682778001711179,
    -0.0638353973311301,-0.0823306940058688), Vec3(0), *femur_l, Vec3(0),
    Vec3(0));
auto& hip_q_l = hip_l->updCoordinate();
hip_q_l.setRangeMin(-120*SimTK::Pi/180);
hip_q_l.setRangeMax(120*SimTK::Pi/180);
hip_q_l.setName("hip_q_l");
model->addJoint(hip_l);

// Hip right
/*SpatialTransform st_hip_r;
st_hip_r[2].setCoordinateNames(
    OpenSim::Array<std::string>("hip_flexion_r", 1, 1));
st_hip_r[2].setFunction(new LinearFunction());
st_hip_r[2].setAxis(Vec3(0, 0, 1));
auto* hip_r = new CustomJoint("hip_r", *pelvis, Vec3(-0.0682778001711179,
    -0.0638353973311301, 0.0823306940058688), Vec3(0), *femur_r, Vec3(0),
    Vec3(0), st_hip_r);
model->addJoint(hip_r);
hip_r->finalizeFromProperties();
auto& hip_q_r = hip_r->upd_coordinates(0);
hip_q_r.setName("hip_q_r");*/
auto* hip_r = new PinJoint("hip_r", *pelvis, Vec3(-0.0682778001711179,
    -0.0638353973311301, 0.0823306940058688), Vec3(0), *femur_r, Vec3(0),
    Vec3(0));
auto& hip_q_r = hip_r->updCoordinate();
hip_q_r.setRangeMin(-120*SimTK::Pi/180);
hip_q_r.setRangeMax(120*SimTK::Pi/180);
hip_q_r.setName("hip_q_r");
model->addJoint(hip_r);

// Knee left
/*SpatialTransform st_knee_l;
st_knee_l[2].setCoordinateNames(
    OpenSim::Array<std::string>("knee_angle_l", 1, 1));
st_knee_l[2].setFunction(new LinearFunction());
st_knee_l[2].setAxis(Vec3(0, 0, 1));
auto* knee_l = new CustomJoint("knee_l", *femur_l,
    Vec3(-0.00451221232146798, -0.396907245921447, 0), Vec3(0), *tibia_l,
    Vec3(0), Vec3(0), st_knee_l);
model->addJoint(knee_l);
knee_l->finalizeFromProperties();
auto& knee_q_l = knee_l->upd_coordinates(0);
knee_q_l.setName("knee_q_l");*/
auto* knee_l = new PinJoint("knee_l", *femur_l, Vec3(-0.00451221232146798,
    -0.396907245921447, 0), Vec3(0), *tibia_l, Vec3(0), Vec3(0));
auto& knee_q_l = knee_l->updCoordinate();
knee_q_l.setRangeMin(-120*SimTK::Pi/180);
knee_q_l.setRangeMax(10*SimTK::Pi/180);
knee_q_l.setName("knee_q_l");
model->addJoint(knee_l);

// Knee right
/*SpatialTransform st_knee_r;
st_knee_r[2].setCoordinateNames(
    OpenSim::Array<std::string>("knee_angle_r", 1, 1));
st_knee_r[2].setFunction(new LinearFunction());
st_knee_r[2].setAxis(Vec3(0, 0, 1));
auto* knee_r = new CustomJoint("knee_r", *femur_r,
    Vec3(-0.00451221232146798, -0.396907245921447, 0), Vec3(0), *tibia_r,
    Vec3(0), Vec3(0), st_knee_r);
model->addJoint(knee_r);
knee_r->finalizeFromProperties();
auto& knee_q_r = knee_r->upd_coordinates(0);
knee_q_r.setName("knee_q_r");*/
auto* knee_r = new PinJoint("knee_r", *femur_r, Vec3(-0.00451221232146798,
    -0.396907245921447, 0), Vec3(0), *tibia_r, Vec3(0), Vec3(0));
auto& knee_q_r = knee_r->updCoordinate();
knee_q_r.setRangeMin(-120*SimTK::Pi/180);
knee_q_r.setRangeMax(10*SimTK::Pi/180);
knee_q_r.setName("knee_q_r");
model->addJoint(knee_r);

// Ankle left
auto* ankle_l = new PinJoint("ankle_l", *tibia_l,
    Vec3(0, -0.415694825374905, 0), Vec3(0), *talus_l, Vec3(0), Vec3(0));
auto& ankle_q_l = ankle_l->updCoordinate();
ankle_q_l.setRangeMin(-SimTK::Pi/2);
ankle_q_l.setRangeMax(SimTK::Pi/2);
ankle_q_l.setName("ankle_q_l");
model->addJoint(ankle_l);
// Ankle right
auto* ankle_r = new PinJoint("ankle_r", *tibia_r,
    Vec3(0, -0.415694825374905, 0), Vec3(0), *talus_r, Vec3(0), Vec3(0));
auto& ankle_q_r = ankle_r->updCoordinate();
ankle_q_r.setRangeMin(-SimTK::Pi/2);
ankle_q_r.setRangeMax(SimTK::Pi/2);
ankle_q_r.setName("ankle_q_r");
model->addJoint(ankle_r);
// Subtalar left
auto* subtalar_l = new WeldJoint("subtalar_l", *talus_l,
    Vec3(-0.0445720919117321, -0.0383391276542374, -0.00723828107321956),
    Vec3(0), *calcn_l, Vec3(0), Vec3(0));
model->addJoint(subtalar_l);
// Subtalar right
auto* subtalar_r = new WeldJoint("subtalar_r", *talus_r,
    Vec3(-0.0445720919117321, -0.0383391276542374, 0.00723828107321956),
    Vec3(0), *calcn_r, Vec3(0), Vec3(0));
model->addJoint(subtalar_r);
// MTP left
auto* mtp_l = new WeldJoint("mtp_l", *calcn_l,
    Vec3(0.163409678774199, -0.00182784875586352, -0.000987038328166303),
    Vec3(0), *toes_l, Vec3(0), Vec3(0));
model->addJoint(mtp_l);
 // MTP right
auto* mtp_r = new WeldJoint("mtp_r", *calcn_r,
    Vec3(0.163409678774199, -0.00182784875586352, 0.000987038328166303),
    Vec3(0), *toes_r, Vec3(0), Vec3(0));
model->addJoint(mtp_r);
// Lumbar
auto* lumbar = new PinJoint("lumbar", *pelvis,
    Vec3(-0.0972499926058214, 0.0787077894476112, 0), Vec3(0),
    *torso, Vec3(0), Vec3(0));
auto& lumbar_q = lumbar->updCoordinate();
lumbar_q.setRangeMin(-SimTK::Pi/2);
lumbar_q.setRangeMax(SimTK::Pi/2);
lumbar_q.setName("lumbar_q");
model->addJoint(lumbar);

///////////////////////////////////////////////////////////////////////////
//Add contact models
///////////////////////////////////////////////////////////////////////////
double contactSphereHeelRadius = 0.035;
double contactSphereFrontRadius = 0.015;
double contactSphereHeelStiffness = 3067776;
double contactSphereFrontStiffness = 3067776;
double dissipation = 2.0;
double staticFriction = 0.8;
double dynamicFriction = 0.8;
double viscousFriction = 0.5;
double transitionVelocity = 0.2;
double cf = 1e-5;
double bd = 300;
double bv = 50;
Vec3 locSphereHeel_l(0.031307527581931796, 0.010435842527310599, 0);
Vec3 locSphereFront_l(0.1774093229642802, -0.015653763790965898,
    -0.005217921263655299);
Vec3 locSphereHeel_r(0.031307527581931796, 0.010435842527310599, 0);
Vec3 locSphereFront_r(0.1774093229642802, -0.015653763790965898,
    0.005217921263655299);
Vec3 halfSpaceOrientation(0,0,-0.5*SimTK::Pi);
Vec3 halfSpaceLocation(0);

auto* HC_heel_r = new SmoothSphereHalfSpaceForce("contactSphereHeel_r",
    *calcn_r, locSphereHeel_r,contactSphereHeelRadius,model->getGround(),
    halfSpaceLocation,halfSpaceOrientation);
HC_heel_r->setName("contactSphereHeel_r");
HC_heel_r->set_stiffness(contactSphereHeelStiffness);
HC_heel_r->set_dissipation(dissipation);
HC_heel_r->set_static_friction(staticFriction);
HC_heel_r->set_dynamic_friction(dynamicFriction);
HC_heel_r->set_viscous_friction(viscousFriction);
HC_heel_r->set_transition_velocity(transitionVelocity);
HC_heel_r->set_derivative_smoothing(cf);
HC_heel_r->set_hertz_smoothing(bd);
HC_heel_r->set_hunt_crossley_smoothing(bv);
model->addComponent(HC_heel_r);

Sphere bodyGeometry1(contactSphereHeelRadius);
bodyGeometry1.setColor(SimTK::Blue);
PhysicalOffsetFrame* ballCenter1 = new PhysicalOffsetFrame(
    "ballCenter1", *calcn_r, Transform(Rotation(SimTK::BodyRotationSequence,
    0, SimTK::XAxis, 0, SimTK::YAxis, 0, SimTK::ZAxis), locSphereHeel_r));
calcn_r->addComponent(ballCenter1);
ballCenter1->attachGeometry(bodyGeometry1.clone());
/*HC_heel_r->connectSocket_body_contact_sphere(*calcn_r);
HC_heel_r->connectSocket_body_contact_half_space(model->getGround());*/

auto* HC_heel_l = new SmoothSphereHalfSpaceForce("contactSphereHeel_l",
    *calcn_l, locSphereHeel_l,contactSphereHeelRadius,model->getGround(),
    halfSpaceLocation,halfSpaceOrientation);
HC_heel_l->setName("contactSphereHeel_l");
HC_heel_l->set_stiffness(contactSphereHeelStiffness);
HC_heel_l->set_dissipation(dissipation);
HC_heel_l->set_static_friction(staticFriction);
HC_heel_l->set_dynamic_friction(dynamicFriction);
HC_heel_l->set_viscous_friction(viscousFriction);
HC_heel_l->set_transition_velocity(transitionVelocity);
HC_heel_l->set_derivative_smoothing(cf);
HC_heel_l->set_hertz_smoothing(bd);
HC_heel_l->set_hunt_crossley_smoothing(bv);

Sphere bodyGeometry2(contactSphereHeelRadius);
bodyGeometry2.setColor(SimTK::Blue);
PhysicalOffsetFrame* ballCenter2 = new PhysicalOffsetFrame(
    "ballCenter2", *calcn_l, Transform(Rotation(SimTK::BodyRotationSequence,
    0, SimTK::XAxis, 0, SimTK::YAxis, 0, SimTK::ZAxis), locSphereHeel_l));
calcn_l->addComponent(ballCenter2);
ballCenter2->attachGeometry(bodyGeometry2.clone());
/*HC_heel_l->connectSocket_body_contact_sphere(*calcn_l);
HC_heel_l->connectSocket_body_contact_half_space(model->getGround());*/

auto* HC_front_r = new SmoothSphereHalfSpaceForce("contactSphereFront_r",
    *calcn_r, locSphereFront_r,contactSphereFrontRadius,model->getGround(),
    halfSpaceLocation,halfSpaceOrientation);
HC_front_r->setName("contactSphereFront_r");
HC_front_r->set_stiffness(contactSphereFrontStiffness);
HC_front_r->set_dissipation(dissipation);
HC_front_r->set_static_friction(staticFriction);
HC_front_r->set_dynamic_friction(dynamicFriction);
HC_front_r->set_viscous_friction(viscousFriction);
HC_front_r->set_transition_velocity(transitionVelocity);
HC_front_r->set_derivative_smoothing(cf);
HC_front_r->set_hertz_smoothing(bd);
HC_front_r->set_hunt_crossley_smoothing(bv);
model->addComponent(HC_front_r);

Sphere bodyGeometry3(contactSphereFrontRadius);
bodyGeometry3.setColor(SimTK::Blue);
PhysicalOffsetFrame* ballCenter3 = new PhysicalOffsetFrame(
    "ballCenter3", *calcn_r, Transform(Rotation(SimTK::BodyRotationSequence,
    0, SimTK::XAxis, 0, SimTK::YAxis, 0, SimTK::ZAxis), locSphereFront_r));
calcn_r->addComponent(ballCenter3);
ballCenter3->attachGeometry(bodyGeometry3.clone());
/*HC_front_r->connectSocket_body_contact_sphere(*calcn_r);
HC_front_r->connectSocket_body_contact_half_space(model->getGround());*/

auto* HC_front_l = new SmoothSphereHalfSpaceForce("contactSphereFront_l",
    *calcn_l, locSphereFront_l,contactSphereFrontRadius,model->getGround(),
    halfSpaceLocation,halfSpaceOrientation);
HC_front_l->setName("contactSphereFront_l");
HC_front_l->set_stiffness(contactSphereFrontStiffness);
HC_front_l->set_dissipation(dissipation);
HC_front_l->set_static_friction(staticFriction);
HC_front_l->set_dynamic_friction(dynamicFriction);
HC_front_l->set_viscous_friction(viscousFriction);
HC_front_l->set_transition_velocity(transitionVelocity);
HC_front_l->set_derivative_smoothing(cf);
HC_front_l->set_hertz_smoothing(bd);
HC_front_l->set_hunt_crossley_smoothing(bv);
model->addComponent(HC_front_l);

Sphere bodyGeometry4(contactSphereFrontRadius);
bodyGeometry4.setColor(SimTK::Blue);
PhysicalOffsetFrame* ballCenter4 = new PhysicalOffsetFrame(
    "ballCenter4", *calcn_l, Transform(Rotation(SimTK::BodyRotationSequence,
    0, SimTK::XAxis, 0, SimTK::YAxis, 0, SimTK::ZAxis), locSphereFront_l));
calcn_l->addComponent(ballCenter4);
ballCenter4->attachGeometry(bodyGeometry4.clone());
/*HC_front_l->connectSocket_body_contact_sphere(*calcn_l);
HC_front_l->connectSocket_body_contact_half_space(model->getGround());*/

///////////////////////////////////////////////////////////////////////////
// Add coordinate actuators
///////////////////////////////////////////////////////////////////////////
// Ground pelvis
// RotationZ
auto* groundPelvisAct_rz = new CoordinateActuator();
groundPelvisAct_rz->setCoordinate(
    &groundPelvis->updCoordinate(PlanarJoint::Coord::RotationZ));
groundPelvisAct_rz->setName("groundPelvisAct_rz");
groundPelvisAct_rz->setOptimalForce(1);
groundPelvisAct_rz->setMinControl(-150);
groundPelvisAct_rz->setMaxControl(150);
model->addComponent(groundPelvisAct_rz);
// TranslationX
auto* groundPelvisAct_tx = new CoordinateActuator();
groundPelvisAct_tx->setCoordinate(
    &groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationX));
groundPelvisAct_tx->setName("groundPelvisAct_tx");
groundPelvisAct_tx->setOptimalForce(1);
groundPelvisAct_tx->setMinControl(-150);
groundPelvisAct_tx->setMaxControl(150);
model->addComponent(groundPelvisAct_tx);
// TranslationY
auto* groundPelvisAct_ty = new CoordinateActuator();
groundPelvisAct_ty->setCoordinate(
    &groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationY));
groundPelvisAct_ty->setName("groundPelvisAct_ty");
groundPelvisAct_ty->setOptimalForce(1);
groundPelvisAct_ty->setMinControl(-150);
groundPelvisAct_ty->setMaxControl(150);
model->addComponent(groundPelvisAct_ty);

// Hip left
auto* hipAct_l = new CoordinateActuator();
hipAct_l->setCoordinate(&hip_l->updCoordinate());
hipAct_l->setName("hipAct_l");
hipAct_l->setOptimalForce(1);
hipAct_l->setMinControl(-150);
hipAct_l->setMaxControl(150);
model->addComponent(hipAct_l);

// Hip right
auto* hipAct_r = new CoordinateActuator();
hipAct_r->setCoordinate(&hip_r->updCoordinate());
hipAct_r->setName("hipAct_r");
hipAct_r->setOptimalForce(1);
hipAct_r->setMinControl(-150);
hipAct_r->setMaxControl(150);
model->addComponent(hipAct_r);

//// Knee left
auto* kneeAct_l = new CoordinateActuator();
kneeAct_l->setCoordinate(&knee_l->updCoordinate());
kneeAct_l->setName("kneeAct_l");
kneeAct_l->setOptimalForce(1);
kneeAct_l->setMinControl(-150);
kneeAct_l->setMaxControl(150);
model->addComponent(kneeAct_l);

// Knee right
auto* kneeAct_r = new CoordinateActuator();
kneeAct_r->setCoordinate(&knee_r->updCoordinate());
kneeAct_r->setName("kneeAct_r");
kneeAct_r->setOptimalForce(1);
kneeAct_r->setMinControl(-150);
kneeAct_r->setMaxControl(150);
model->addComponent(kneeAct_r);;

// Ankle left
auto* ankleAct_l = new CoordinateActuator();
ankleAct_l->setCoordinate(&ankle_l->updCoordinate());
ankleAct_l->setName("ankleAct_l");
ankleAct_l->setOptimalForce(1);
ankleAct_l->setMinControl(-150);
ankleAct_l->setMaxControl(150);
model->addComponent(ankleAct_l);

// Ankle right
auto* ankleAct_r = new CoordinateActuator();
ankleAct_r->setCoordinate(&ankle_r->updCoordinate());
ankleAct_r->setName("ankleAct_r");
ankleAct_r->setOptimalForce(1);
ankleAct_r->setMinControl(-150);
ankleAct_r->setMaxControl(150);
model->addComponent(ankleAct_r);

// Lumbar
auto* lumbarAct = new CoordinateActuator();
lumbarAct->setCoordinate(&lumbar->updCoordinate());
lumbarAct->setName("lumbarAct");
lumbarAct->setOptimalForce(1);
lumbarAct->setMinControl(-150);
lumbarAct->setMaxControl(150);
model->addComponent(lumbarAct);

////// Display geometry
//Sphere bodyGeometry(0.01);
//bodyGeometry.setColor(SimTK::Gray);
//Sphere bodyGeometry1(0.01);
//bodyGeometry1.setColor(SimTK::Blue);
//
//pelvis->attachGeometry(bodyGeometry1.clone());
////femur_l->attachGeometry(bodyGeometry.clone());
////femur_r->attachGeometry(bodyGeometry.clone());
////tibia_l->attachGeometry(bodyGeometry.clone());
////tibia_r->attachGeometry(bodyGeometry.clone());
////talus_l->attachGeometry(bodyGeometry.clone());
////talus_r->attachGeometry(bodyGeometry.clone());
////calcn_l->attachGeometry(bodyGeometry.clone());
////calcn_r->attachGeometry(bodyGeometry.clone());
////toes_l->attachGeometry(bodyGeometry.clone());
////toes_r->attachGeometry(bodyGeometry.clone());
////torso->attachGeometry(bodyGeometry.clone());


model->finalizeConnections();

model->print("gait_2D_contact_torque_geom.osim");

//int ndof = model->getNumStateVariables()/2;
//SimTK::Vector QsUs(2*ndof);
//QsUs.setTo(-2.);

//SimTK::Vector knownUdot(ndof);
//knownUdot.setTo(-2.);

//model->setStateVariableValues(*state, QsUs);
//model->realizeVelocity(*state);

//// Compute residual forces
///// appliedMobilityForces (# mobilities)
//SimTK::Vector appliedMobilityForces(ndof);
//appliedMobilityForces.setToZero();
///// appliedBodyForces (# bodies + ground)
//SimTK::Vector_<SimTK::SpatialVec> appliedBodyForces;
//int nbodies = model->getBodySet().getSize() + 1; // including ground
//appliedBodyForces.resize(nbodies);
//appliedBodyForces.setToZero();
///// gravity
//Vec3 gravity(0);
//gravity[1] = -9.81;
//for (int i = 0; i < model->getBodySet().getSize(); ++i) {
//    model->getMatterSubsystem().addInStationForce(*state,
//        model->getBodySet().get(i).getMobilizedBodyIndex(),
//        model->getBodySet().get(i).getMassCenter(),
//        model->getBodySet().get(i).getMass()*gravity, appliedBodyForces);
//}
///// contact forces
///// right
//Array<double> Force_values_heel_r = HC_heel_r->getRecordValues(*state);
//Array<double> Force_values_front_r = HC_front_r->getRecordValues(*state);
//SimTK::SpatialVec GRF_heel_r;
//GRF_heel_r[0] = Vec3(Force_values_heel_r[3], Force_values_heel_r[4],
//    Force_values_heel_r[5]);
//GRF_heel_r[1] = Vec3(Force_values_heel_r[0], Force_values_heel_r[1],
//    Force_values_heel_r[2]);
//SimTK::SpatialVec GRF_front_r;
//GRF_front_r[0] = Vec3(Force_values_front_r[3], Force_values_front_r[4],
//    Force_values_front_r[5]);
//GRF_front_r[1] = Vec3(Force_values_front_r[0], Force_values_front_r[1],
//    Force_values_front_r[2]);
//int nfoot_r = model->getBodySet().get("calcn_r").getMobilizedBodyIndex();
//appliedBodyForces[nfoot_r] =
//    appliedBodyForces[nfoot_r] + GRF_heel_r + GRF_front_r;
///// left
//Array<double> Force_values_heel_l = HC_heel_l->getRecordValues(*state);
//Array<double> Force_values_front_l = HC_front_l->getRecordValues(*state);
//SimTK::SpatialVec GRF_heel_l;
//GRF_heel_l[0] = Vec3(Force_values_heel_l[3], Force_values_heel_l[4],
//    Force_values_heel_l[5]);
//GRF_heel_l[1] = Vec3(Force_values_heel_l[0], Force_values_heel_l[1],
//    Force_values_heel_l[2]);
//SimTK::SpatialVec GRF_front_l;
//GRF_front_l[0] = Vec3(Force_values_front_l[3], Force_values_front_l[4],
//    Force_values_front_l[5]);
//GRF_front_l[1] = Vec3(Force_values_front_l[0], Force_values_front_l[1],
//    Force_values_front_l[2]);
//int nfoot_l = model->getBodySet().get("calcn_l").getMobilizedBodyIndex();
//appliedBodyForces[nfoot_l] =
//    appliedBodyForces[nfoot_l] + GRF_heel_l + GRF_front_l;
///// inverse dynamics
//SimTK::Vector residualMobilityForces(ndof);
//residualMobilityForces.setToZero();
//model->getMatterSubsystem().calcResidualForceIgnoringConstraints(
//    *state, appliedMobilityForces, appliedBodyForces, knownUdot,
//    residualMobilityForces);

//SimTK::SpatialVec GRF_r = GRF_heel_r + GRF_front_r;
//SimTK::SpatialVec GRF_l = GRF_heel_l + GRF_front_l;

//for (int i = 0; i < 5; ++i) {
//    std::cout << residualMobilityForces[i] << std::endl;
//}
//// Order adjusted since order Simbody is different than order OpenSim
//std::cout << residualMobilityForces[6] << std::endl;
//std::cout << residualMobilityForces[7] << std::endl;
//std::cout << residualMobilityForces[8] << std::endl;
//std::cout << residualMobilityForces[9] << std::endl;
//std::cout << residualMobilityForces[5] << std::endl;

//std::cout << residualMobilityForces[7] << std::endl;
//std::cout << residualMobilityForces[8] << std::endl;
//std::cout << residualMobilityForces[9] << std::endl;
//std::cout << residualMobilityForces[5] << std::endl;

//for (int i = 0; i < 2; ++i) {
//    std::cout << GRF_r[1][i] << std::endl;
//}
//for (int i = 0; i < 2; ++i) {
//    std::cout << GRF_l[1][i] << std::endl;
//}

//return 0;




    return model;
}

int main() {

    MocoStudy moco;
    moco.setName("gait2D_Predictive");

    // Define the optimal control problem.
    // ===================================
    MocoProblem& problem = moco.updProblem();

    // Model (dynamics).
    // -----------------
    problem.setModel(createGait2D());

    // Bounds.
    // -------

    // States: joint positions and velocities
    // Ground pelvis
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_rz/value", {-10, 10});
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_rz/speed", {-10, 10});
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_tx/value", {0, 10},{0});
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_tx/speed", {-10, 10});
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_ty/value", {-10, 10});
    problem.setStateInfo("/jointset/groundPelvis/groundPelvis_q_ty/speed", {-10, 10});
    // Hip left
    problem.setStateInfo("/jointset/hip_l/hip_q_l/value", {-10, 10});
    problem.setStateInfo("/jointset/hip_l/hip_q_l/speed", {-10, 10});
    // Hip right
    problem.setStateInfo("/jointset/hip_r/hip_q_r/value", {-10, 10});
    problem.setStateInfo("/jointset/hip_r/hip_q_r/speed", {-10, 10});
    // Knee left
    problem.setStateInfo("/jointset/knee_l/knee_q_l/value", {-10, 10});
    problem.setStateInfo("/jointset/knee_l/knee_q_l/speed", {-10, 10});
    // Knee right
    problem.setStateInfo("/jointset/knee_r/knee_q_r/value", {-10, 10});
    problem.setStateInfo("/jointset/knee_r/knee_q_r/speed", {-10, 10});
    // Ankle left
    problem.setStateInfo("/jointset/ankle_l/ankle_q_l/value", {-10, 10});
    problem.setStateInfo("/jointset/ankle_l/ankle_q_l/speed", {-10, 10});
    // Ankle right
    problem.setStateInfo("/jointset/ankle_r/ankle_q_r/value", {-10, 10});
    problem.setStateInfo("/jointset/ankle_r/ankle_q_r/speed", {-10, 10});
    // Lumbar
    problem.setStateInfo("/jointset/lumbar/lumbar_q/value", {-10, 10});
    problem.setStateInfo("/jointset/lumbar/lumbar_q/speed", {-10, 10});

    // Controls: torque actuators
    problem.setControlInfo("/groundPelvisAct_rz", {-150, 150});
    problem.setControlInfo("/groundPelvisAct_tx", {-150, 150});
    problem.setControlInfo("/groundPelvisAct_ty", {-150, 150});
    problem.setControlInfo("/hipAct_l", {-150, 150});
    problem.setControlInfo("/hipAct_r", {-150, 150});
    problem.setControlInfo("/kneeAct_l", {-150, 150});
    problem.setControlInfo("/kneeAct_r", {-150, 150});
    problem.setControlInfo("/ankleAct_l", {-150, 150});
    problem.setControlInfo("/ankleAct_r", {-150, 150});
    problem.setControlInfo("/lumbarAct", {-150, 150});

    // Static parameter: final time
    double finalTime = 1.0;
    problem.setTimeBounds(0, finalTime);

    //// Cost.
    //// -----
    // Minimize torque actuators squared
    auto* controlCost = problem.addCost<MocoControlCost>("controlCost");
    controlCost->set_weight(1);

    // Impose average speed
    auto* speedCost = problem.addCost<MocoAverageSpeedCost>("speedCost");
    speedCost->set_weight(1);
    speedCost->set_desired_speed(1.2);

    // Impose symmetry
    // TODO

    // Impose 1/d * squared controls

    // Configure the solver.
    // =====================
    auto& solver = moco.initCasADiSolver();
    solver.set_num_mesh_points(50);
    solver.set_verbosity(2);
    solver.set_optim_solver("ipopt");

    moco.print("gait2D_Predictive.omoco");

    // Solve the problem.
    // ==================
    MocoSolution solution = moco.solve();
    solution.write("gait2D_Predictive_solution.sto");

    moco.visualize(solution);

    return EXIT_SUCCESS;

return 0;
}

//model->setName("gait_2D");

//using SimTK::Vec3;
//using SimTK::Inertia;
//using SimTK::Transform;

//// Create model
/////////////////////////////////////////////////////////////////////////////
//// Add bodies
/////////////////////////////////////////////////////////////////////////////
//auto* pelvis = new OpenSim::Body("pelvis", 9.7143336091724,
//    Vec3(-0.0682778,0,0), Inertia(0.0814928846050306,0.0814928846050306,
//    0.0445427591530667,0,0,0));
//model->addBody(pelvis);
//auto* femur_l = new OpenSim::Body("femur_l", 7.67231915023828,
//    Vec3(0,-0.170467,0), Inertia(0.111055472890139,0.0291116288158616,
//    0.117110028170931,0,0,0));
//model->addBody(femur_l);
//auto* femur_r = new OpenSim::Body("femur_r", 7.67231915023828,
//    Vec3(0,-0.170467,0), Inertia(0.111055472890139,0.0291116288158616,
//    0.117110028170931,0,0,0));
//model->addBody(femur_r);
//auto* tibia_l = new OpenSim::Body("tibia_l", 3.05815503574821,
//    Vec3(0,-0.180489,0), Inertia(0.0388526996597354,0.00393152317985418,
//    0.0393923204883429,0,0,0));
//model->addBody(tibia_l);
//auto* tibia_r = new OpenSim::Body("tibia_r", 3.05815503574821,
//    Vec3(0,-0.180489,0), Inertia(0.0388526996597354,0.00393152317985418,
//    0.0393923204883429,0,0,0));
//model->addBody(tibia_r);
//auto* talus_l = new OpenSim::Body("talus_l", 0.082485638186061,
//    Vec3(0), Inertia(0.000688967700910182,0.000688967700910182,
//    0.000688967700910182,0,0,0));
//model->addBody(talus_l);
//auto* talus_r = new OpenSim::Body("talus_r", 0.082485638186061,
//    Vec3(0), Inertia(0.000688967700910182,0.000688967700910182,
//    0.000688967700910182,0,0,0));
//model->addBody(talus_r);
//auto* calcn_l = new OpenSim::Body("calcn_l", 1.03107047732576,
//    Vec3(0.0913924,0.0274177,0), Inertia(0.000964554781274254,
//    0.00268697403354971,0.00282476757373175,0,0,0));
//model->addBody(calcn_l);
//auto* calcn_r = new OpenSim::Body("calcn_r", 1.03107047732576,
//    Vec3(0.0913924,0.0274177,0), Inertia(0.000964554781274254,
//    0.00268697403354971,0.00282476757373175,0,0,0));
//model->addBody(calcn_r);
//auto* toes_l = new OpenSim::Body("toes_l", 0.178663892311008,
//    Vec3(0.0316218,0.00548355,0.0159937), Inertia(6.88967700910182e-005,
//    0.000137793540182036,6.88967700910182e-005,0,0,0));
//model->addBody(toes_l);
//auto* toes_r = new OpenSim::Body("toes_r", 0.178663892311008,
//    Vec3(0.0316218,0.00548355,-0.0159937), Inertia(6.88967700910182e-005,
//    0.000137793540182036,6.88967700910182e-005,0,0,0));
//model->addBody(toes_r);
//auto* torso = new OpenSim::Body("torso", 28.240278003209,
//    Vec3(-0.0289722,0.309037,0), Inertia(1.14043571182129,
//    0.593400919285897,1.14043571182129,0,0,0));
//model->addBody(torso);

/////////////////////////////////////////////////////////////////////////////
//// Add joints
/////////////////////////////////////////////////////////////////////////////
//// Ground pelvis
//auto* groundPelvis = new PlanarJoint("groundPelvis", model->getGround(),
//    Vec3(0), Vec3(0), *pelvis, Vec3(0), Vec3(0));
//auto& groundPelvis_q_rz =
//    groundPelvis->updCoordinate(PlanarJoint::Coord::RotationZ);
//groundPelvis_q_rz.setName("groundPelvis_q_rz");
//auto& groundPelvis_q_tx =
//    groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationX);
//groundPelvis_q_tx.setName("groundPelvis_q_tx");
//auto& groundPelvis_q_ty =
//    groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationY);
//groundPelvis_q_ty.setName("groundPelvis_q_ty");
//model->addJoint(groundPelvis);

//// Hip left
///*SpatialTransform st_hip_l;
//st_hip_l[2].setCoordinateNames(
//    OpenSim::Array<std::string>("hip_flexion_l", 1, 1));
//st_hip_l[2].setFunction(new LinearFunction());
//st_hip_l[2].setAxis(Vec3(0, 0, 1));
//auto* hip_l = new CustomJoint("hip_l", *pelvis, Vec3(-0.0682778001711179,
//    -0.0638353973311301,-0.0823306940058688), Vec3(0), *femur_l, Vec3(0),
//    Vec3(0), st_hip_l);
//model->addJoint(hip_l);
//hip_l->finalizeFromProperties();
//auto& hip_q_l = hip_l->upd_coordinates(0);
//hip_q_l.setName("hip_q_l");*/
//auto* hip_l = new PinJoint("hip_l", *pelvis, Vec3(-0.0682778001711179,
//    -0.0638353973311301,-0.0823306940058688), Vec3(0), *femur_l, Vec3(0),
//    Vec3(0));
//auto& hip_q_l = hip_l->updCoordinate();
//hip_q_l.setName("hip_q_l");
//model->addJoint(hip_l);

//// Hip right
///*SpatialTransform st_hip_r;
//st_hip_r[2].setCoordinateNames(
//    OpenSim::Array<std::string>("hip_flexion_r", 1, 1));
//st_hip_r[2].setFunction(new LinearFunction());
//st_hip_r[2].setAxis(Vec3(0, 0, 1));
//auto* hip_r = new CustomJoint("hip_r", *pelvis, Vec3(-0.0682778001711179,
//    -0.0638353973311301, 0.0823306940058688), Vec3(0), *femur_r, Vec3(0),
//    Vec3(0), st_hip_r);
//model->addJoint(hip_r);
//hip_r->finalizeFromProperties();
//auto& hip_q_r = hip_r->upd_coordinates(0);
//hip_q_r.setName("hip_q_r");*/
//auto* hip_r = new PinJoint("hip_r", *pelvis, Vec3(-0.0682778001711179,
//    -0.0638353973311301, 0.0823306940058688), Vec3(0), *femur_r, Vec3(0),
//    Vec3(0));
//auto& hip_q_r = hip_r->updCoordinate();
//hip_q_r.setName("hip_q_r");
//model->addJoint(hip_r);

//// Knee left
///*SpatialTransform st_knee_l;
//st_knee_l[2].setCoordinateNames(
//    OpenSim::Array<std::string>("knee_angle_l", 1, 1));
//st_knee_l[2].setFunction(new LinearFunction());
//st_knee_l[2].setAxis(Vec3(0, 0, 1));
//auto* knee_l = new CustomJoint("knee_l", *femur_l,
//    Vec3(-0.00451221232146798, -0.396907245921447, 0), Vec3(0), *tibia_l,
//    Vec3(0), Vec3(0), st_knee_l);
//model->addJoint(knee_l);
//knee_l->finalizeFromProperties();
//auto& knee_q_l = knee_l->upd_coordinates(0);
//knee_q_l.setName("knee_q_l");*/
//auto* knee_l = new PinJoint("knee_l", *femur_l, Vec3(-0.00451221232146798,
//    -0.396907245921447, 0), Vec3(0), *tibia_l, Vec3(0), Vec3(0));
//auto& knee_q_l = knee_l->updCoordinate();
//knee_q_l.setName("knee_q_l");
//model->addJoint(knee_l);

//// Knee right
///*SpatialTransform st_knee_r;
//st_knee_r[2].setCoordinateNames(
//    OpenSim::Array<std::string>("knee_angle_r", 1, 1));
//st_knee_r[2].setFunction(new LinearFunction());
//st_knee_r[2].setAxis(Vec3(0, 0, 1));
//auto* knee_r = new CustomJoint("knee_r", *femur_r,
//    Vec3(-0.00451221232146798, -0.396907245921447, 0), Vec3(0), *tibia_r,
//    Vec3(0), Vec3(0), st_knee_r);
//model->addJoint(knee_r);
//knee_r->finalizeFromProperties();
//auto& knee_q_r = knee_r->upd_coordinates(0);
//knee_q_r.setName("knee_q_r");*/
//auto* knee_r = new PinJoint("knee_r", *femur_r, Vec3(-0.00451221232146798,
//    -0.396907245921447, 0), Vec3(0), *tibia_r, Vec3(0), Vec3(0));
//auto& knee_q_r = knee_r->updCoordinate();
//knee_q_r.setName("knee_q_r");
//model->addJoint(knee_r);

//// Ankle left
//auto* ankle_l = new PinJoint("ankle_l", *tibia_l,
//    Vec3(0, -0.415694825374905, 0), Vec3(0), *talus_l, Vec3(0), Vec3(0));
//auto& ankle_q_l = ankle_l->updCoordinate();
//ankle_q_l.setName("ankle_q_l");
//model->addJoint(ankle_l);
//// Ankle right
//auto* ankle_r = new PinJoint("ankle_r", *tibia_r,
//    Vec3(0, -0.415694825374905, 0), Vec3(0), *talus_r, Vec3(0), Vec3(0));
//auto& ankle_q_r = ankle_r->updCoordinate();
//ankle_q_r.setName("ankle_q_r");
//model->addJoint(ankle_r);
//// Subtalar left
//auto* subtalar_l = new WeldJoint("subtalar_l", *talus_l,
//    Vec3(-0.0445720919117321, -0.0383391276542374, -0.00723828107321956),
//    Vec3(0), *calcn_l, Vec3(0), Vec3(0));
//model->addJoint(subtalar_l);
//// Subtalar right
//auto* subtalar_r = new WeldJoint("subtalar_r", *talus_r,
//    Vec3(-0.0445720919117321, -0.0383391276542374, 0.00723828107321956),
//    Vec3(0), *calcn_r, Vec3(0), Vec3(0));
//model->addJoint(subtalar_r);
//// MTP left
//auto* mtp_l = new WeldJoint("mtp_l", *calcn_l,
//    Vec3(0.163409678774199, -0.00182784875586352, -0.000987038328166303),
//    Vec3(0), *toes_l, Vec3(0), Vec3(0));
//model->addJoint(mtp_l);
// // MTP right
//auto* mtp_r = new WeldJoint("mtp_r", *calcn_r,
//    Vec3(0.163409678774199, -0.00182784875586352, 0.000987038328166303),
//    Vec3(0), *toes_r, Vec3(0), Vec3(0));
//model->addJoint(mtp_r);
//// Lumbar
//auto* lumbar = new PinJoint("lumbar", *pelvis,
//    Vec3(-0.0972499926058214, 0.0787077894476112, 0), Vec3(0),
//    *torso, Vec3(0), Vec3(0));
//auto& lumbar_q = lumbar->updCoordinate();
//lumbar_q.setName("lumbar_q");
//model->addJoint(lumbar);

/////////////////////////////////////////////////////////////////////////////
////Add contact models
/////////////////////////////////////////////////////////////////////////////
//double contactSphereHeelRadius = 0.035;
//double contactSphereFrontRadius = 0.015;
//double contactSphereHeelStiffness = 3067776;
//double contactSphereFrontStiffness = 3067776;
//double dissipation = 2.0;
//double staticFriction = 0.8;
//double dynamicFriction = 0.8;
//double viscousFriction = 0.5;
//double transitionVelocity = 0.2;
//double cf = 1e-5;
//double bd = 300;
//double bv = 50;
//Vec3 locSphereHeel_l(0.031307527581931796, 0.010435842527310599, 0);
//Vec3 locSphereFront_l(0.1774093229642802, -0.015653763790965898,
//    -0.005217921263655299);
//Vec3 locSphereHeel_r(0.031307527581931796, 0.010435842527310599, 0);
//Vec3 locSphereFront_r(0.1774093229642802, -0.015653763790965898,
//    0.005217921263655299);
//Transform halfSpaceFrame(Rotation(0, SimTK::ZAxis), Vec3(0));

//auto* HC_heel_r = new SmoothSphereHalfSpaceForce("contactSphereHeel_r",
//    *calcn_r, locSphereHeel_r,contactSphereHeelRadius,model->getGround(),
//    halfSpaceFrame,contactSphereHeelStiffness,dissipation,staticFriction,
//    dynamicFriction,viscousFriction,transitionVelocity,cf,bd,bv);
//HC_heel_r->setName("contactSphereHeel_r");
//model->addComponent(HC_heel_r);
///*HC_heel_r->connectSocket_body_contact_sphere(*calcn_r);
//HC_heel_r->connectSocket_body_contact_half_space(model->getGround());*/

//auto* HC_heel_l = new SmoothSphereHalfSpaceForce("contactSphereHeel_l",
//    *calcn_l, locSphereHeel_l,contactSphereHeelRadius,model->getGround(),
//    halfSpaceFrame,contactSphereHeelStiffness,dissipation,staticFriction,
//    dynamicFriction,viscousFriction,transitionVelocity,cf,bd,bv);
//HC_heel_l->setName("contactSphereHeel_l");
//model->addComponent(HC_heel_l);
///*HC_heel_l->connectSocket_body_contact_sphere(*calcn_l);
//HC_heel_l->connectSocket_body_contact_half_space(model->getGround());*/

//auto* HC_front_r = new SmoothSphereHalfSpaceForce("contactSphereFront_r",
//    *calcn_r, locSphereFront_r,contactSphereFrontRadius,model->getGround(),
//    halfSpaceFrame,contactSphereFrontStiffness,dissipation,staticFriction,
//    dynamicFriction,viscousFriction,transitionVelocity,cf,bd,bv);
//HC_front_r->setName("contactSphereFront_r");
//model->addComponent(HC_front_r);
///*HC_front_r->connectSocket_body_contact_sphere(*calcn_r);
//HC_front_r->connectSocket_body_contact_half_space(model->getGround());*/

//auto* HC_front_l = new SmoothSphereHalfSpaceForce("contactSphereFront_l",
//    *calcn_l, locSphereFront_l,contactSphereFrontRadius,model->getGround(),
//    halfSpaceFrame,contactSphereFrontStiffness,dissipation,staticFriction,
//    dynamicFriction,viscousFriction,transitionVelocity,cf,bd,bv);
//HC_front_l->setName("contactSphereFront_l");
//model->addComponent(HC_front_l);
///*HC_front_l->connectSocket_body_contact_sphere(*calcn_l);
//HC_front_l->connectSocket_body_contact_half_space(model->getGround());*/

/////////////////////////////////////////////////////////////////////////////
//// Add coordinate actuators
/////////////////////////////////////////////////////////////////////////////
//// Ground pelvis
//// RotationZ
//auto* groundPelvisAct_rz = new CoordinateActuator();
//groundPelvisAct_rz->setCoordinate(
//    &groundPelvis->updCoordinate(PlanarJoint::Coord::RotationZ));
//groundPelvisAct_rz->setName("groundPelvisAct_rz");
//groundPelvisAct_rz->setOptimalForce(1);
//groundPelvisAct_rz->setMinControl(-150);
//groundPelvisAct_rz->setMaxControl(150);
//model->addComponent(groundPelvisAct_rz);
//// TranslationX
//auto* groundPelvisAct_tx = new CoordinateActuator();
//groundPelvisAct_tx->setCoordinate(
//    &groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationX));
//groundPelvisAct_tx->setName("groundPelvisAct_tx");
//groundPelvisAct_tx->setOptimalForce(1);
//groundPelvisAct_tx->setMinControl(-150);
//groundPelvisAct_tx->setMaxControl(150);
//model->addComponent(groundPelvisAct_tx);
//// TranslationY
//auto* groundPelvisAct_ty = new CoordinateActuator();
//groundPelvisAct_ty->setCoordinate(
//    &groundPelvis->updCoordinate(PlanarJoint::Coord::TranslationY));
//groundPelvisAct_ty->setName("groundPelvisAct_ty");
//groundPelvisAct_ty->setOptimalForce(1);
//groundPelvisAct_ty->setMinControl(-150);
//groundPelvisAct_ty->setMaxControl(150);
//model->addComponent(groundPelvisAct_ty);

//// Hip left
//auto* hipAct_l = new CoordinateActuator();
//hipAct_l->setCoordinate(&hip_l->updCoordinate());
//hipAct_l->setName("hipAct_l");
//hipAct_l->setOptimalForce(1);
//hipAct_l->setMinControl(-150);
//hipAct_l->setMaxControl(150);
//model->addComponent(hipAct_l);

//// Hip right
//auto* hipAct_r = new CoordinateActuator();
//hipAct_r->setCoordinate(&hip_r->updCoordinate());
//hipAct_r->setName("hipAct_r");
//hipAct_r->setOptimalForce(1);
//hipAct_r->setMinControl(-150);
//hipAct_r->setMaxControl(150);
//model->addComponent(hipAct_r);

////// Knee left
//auto* kneeAct_l = new CoordinateActuator();
//kneeAct_l->setCoordinate(&knee_l->updCoordinate());
//kneeAct_l->setName("kneeAct_l");
//kneeAct_l->setOptimalForce(1);
//kneeAct_l->setMinControl(-150);
//kneeAct_l->setMaxControl(150);
//model->addComponent(kneeAct_l);

//// Knee right
//auto* kneeAct_r = new CoordinateActuator();
//kneeAct_r->setCoordinate(&knee_r->updCoordinate());
//kneeAct_r->setName("kneeAct_r");
//kneeAct_r->setOptimalForce(1);
//kneeAct_r->setMinControl(-150);
//kneeAct_r->setMaxControl(150);
//model->addComponent(kneeAct_r);;

//// Ankle left
//auto* ankleAct_l = new CoordinateActuator();
//ankleAct_l->setCoordinate(&ankle_l->updCoordinate());
//ankleAct_l->setName("ankleAct_l");
//ankleAct_l->setOptimalForce(1);
//ankleAct_l->setMinControl(-150);
//ankleAct_l->setMaxControl(150);
//model->addComponent(ankleAct_l);

//// Ankle right
//auto* ankleAct_r = new CoordinateActuator();
//ankleAct_r->setCoordinate(&ankle_r->updCoordinate());
//ankleAct_r->setName("ankleAct_r");
//ankleAct_r->setOptimalForce(1);
//ankleAct_r->setMinControl(-150);
//ankleAct_r->setMaxControl(150);
//model->addComponent(ankleAct_r);

//// Lumbar
//auto* lumbarAct = new CoordinateActuator();
//lumbarAct->setCoordinate(&lumbar->updCoordinate());
//lumbarAct->setName("lumbarAct");
//lumbarAct->setOptimalForce(1);
//lumbarAct->setMinControl(-150);
//lumbarAct->setMaxControl(150);
//model->addComponent(lumbarAct);

////// Display geometry
//Sphere bodyGeometry(0.01);
//bodyGeometry.setColor(SimTK::Gray);
//Sphere bodyGeometry1(0.01);
//bodyGeometry1.setColor(SimTK::Blue);

//pelvis->attachGeometry(bodyGeometry1.clone());
////femur_l->attachGeometry(bodyGeometry.clone());
////femur_r->attachGeometry(bodyGeometry.clone());
////tibia_l->attachGeometry(bodyGeometry.clone());
////tibia_r->attachGeometry(bodyGeometry.clone());
////talus_l->attachGeometry(bodyGeometry.clone());
////talus_r->attachGeometry(bodyGeometry.clone());
////calcn_l->attachGeometry(bodyGeometry.clone());
////calcn_r->attachGeometry(bodyGeometry.clone());
////toes_l->attachGeometry(bodyGeometry.clone());
////toes_r->attachGeometry(bodyGeometry.clone());
////torso->attachGeometry(bodyGeometry.clone());


//model->finalizeConnections();

////model->print("gait_2D.osim");

//int ndof = model->getNumStateVariables()/2;
//SimTK::Vector QsUs(2*ndof);
//QsUs.setTo(-2.);

//SimTK::Vector knownUdot(ndof);
//knownUdot.setTo(-2.);

//model->setStateVariableValues(*state, QsUs);
//model->realizeVelocity(*state);

//// Compute residual forces
///// appliedMobilityForces (# mobilities)
//SimTK::Vector appliedMobilityForces(ndof);
//appliedMobilityForces.setToZero();
///// appliedBodyForces (# bodies + ground)
//SimTK::Vector_<SimTK::SpatialVec> appliedBodyForces;
//int nbodies = model->getBodySet().getSize() + 1; // including ground
//appliedBodyForces.resize(nbodies);
//appliedBodyForces.setToZero();
///// gravity
//Vec3 gravity(0);
//gravity[1] = -9.81;
//for (int i = 0; i < model->getBodySet().getSize(); ++i) {
//    model->getMatterSubsystem().addInStationForce(*state,
//        model->getBodySet().get(i).getMobilizedBodyIndex(),
//        model->getBodySet().get(i).getMassCenter(),
//        model->getBodySet().get(i).getMass()*gravity, appliedBodyForces);
//}
///// contact forces
///// right
//Array<double> Force_values_heel_r = HC_heel_r->getRecordValues(*state);
//Array<double> Force_values_front_r = HC_front_r->getRecordValues(*state);
//SimTK::SpatialVec GRF_heel_r;
//GRF_heel_r[0] = Vec3(Force_values_heel_r[3], Force_values_heel_r[4],
//    Force_values_heel_r[5]);
//GRF_heel_r[1] = Vec3(Force_values_heel_r[0], Force_values_heel_r[1],
//    Force_values_heel_r[2]);
//SimTK::SpatialVec GRF_front_r;
//GRF_front_r[0] = Vec3(Force_values_front_r[3], Force_values_front_r[4],
//    Force_values_front_r[5]);
//GRF_front_r[1] = Vec3(Force_values_front_r[0], Force_values_front_r[1],
//    Force_values_front_r[2]);
//int nfoot_r = model->getBodySet().get("calcn_r").getMobilizedBodyIndex();
//appliedBodyForces[nfoot_r] =
//    appliedBodyForces[nfoot_r] + GRF_heel_r + GRF_front_r;
///// left
//Array<double> Force_values_heel_l = HC_heel_l->getRecordValues(*state);
//Array<double> Force_values_front_l = HC_front_l->getRecordValues(*state);
//SimTK::SpatialVec GRF_heel_l;
//GRF_heel_l[0] = Vec3(Force_values_heel_l[3], Force_values_heel_l[4],
//    Force_values_heel_l[5]);
//GRF_heel_l[1] = Vec3(Force_values_heel_l[0], Force_values_heel_l[1],
//    Force_values_heel_l[2]);
//SimTK::SpatialVec GRF_front_l;
//GRF_front_l[0] = Vec3(Force_values_front_l[3], Force_values_front_l[4],
//    Force_values_front_l[5]);
//GRF_front_l[1] = Vec3(Force_values_front_l[0], Force_values_front_l[1],
//    Force_values_front_l[2]);
//int nfoot_l = model->getBodySet().get("calcn_l").getMobilizedBodyIndex();
//appliedBodyForces[nfoot_l] =
//    appliedBodyForces[nfoot_l] + GRF_heel_l + GRF_front_l;
///// inverse dynamics
//SimTK::Vector residualMobilityForces(ndof);
//residualMobilityForces.setToZero();
//model->getMatterSubsystem().calcResidualForceIgnoringConstraints(
//    *state, appliedMobilityForces, appliedBodyForces, knownUdot,
//    residualMobilityForces);

//SimTK::SpatialVec GRF_r = GRF_heel_r + GRF_front_r;
//SimTK::SpatialVec GRF_l = GRF_heel_l + GRF_front_l;

//for (int i = 0; i < 5; ++i) {
//    std::cout << residualMobilityForces[i] << std::endl;
//}
//// Order adjusted since order Simbody is different than order OpenSim
//std::cout << residualMobilityForces[6] << std::endl;
//std::cout << residualMobilityForces[7] << std::endl;
//std::cout << residualMobilityForces[8] << std::endl;
//std::cout << residualMobilityForces[9] << std::endl;
//std::cout << residualMobilityForces[5] << std::endl;

//std::cout << residualMobilityForces[7] << std::endl;
//std::cout << residualMobilityForces[8] << std::endl;
//std::cout << residualMobilityForces[9] << std::endl;
//std::cout << residualMobilityForces[5] << std::endl;

//for (int i = 0; i < 2; ++i) {
//    std::cout << GRF_r[1][i] << std::endl;
//}
//for (int i = 0; i < 2; ++i) {
//    std::cout << GRF_l[1][i] << std::endl;
//}

//return 0;