#include <OpenSim/OpenSim.h>
#include <GlobalStaticOptimizationSolver.h>
#include <MuscleRedundancySolver.h>
#include <DeGroote2016Muscle.h>
#include <InverseMuscleSolverMotionData.h>
#include <mesh.h>

#include "testing.h"

using namespace OpenSim;

// The objective of this test is to ensure that MRS functions properly with
// multiple muscles and multiple degrees of freedom.

// The horizontal distance between the muscle origins and the origin of the
// global coordinate system (0, 0).
const double WIDTH = 0.2;
const double ACCEL_GRAVITY = 9.81;

// This allows us to print the value of adoubles, although ADOL-C is supposed
// to provide this operator for us (I think Homebrew's version of ADOL-C is
// missing some function definitions).
std::ostream& operator<<(std::ostream& stream, const adouble& v) {
    stream << v.value() << "(a)";
    return stream;
}

/// Move a point mass from a fixed starting state to a fixed end
/// position and velocity, in fixed time, with minimum effort. The point mass
/// has 2 DOFs (x and y translation).
///
///                            |< d >|< d >|
///                    ----------------------
///                             \         /
///                              .       .
///                   left muscle \     / right muscle
///                                .   .
///                                 \ /
///                                  O mass
///
/// Here's a sketch of the problem we solve (rigid tendon, no activ.dynamics)
/// @verbatim
///   minimize   int_t (aL^2 + aR^2) dt
///   subject to xdot = vx                                        kinematics
///              ydot = vy                                        kinematics
///              vxdot = 1/m (-f_tL (d+x)/lmtL + f_tR (d-x)/lmtR) dynamics
///              vydot = 1/m (-f_tL (-y)/lmtL + f_tR (-y)/lmtR)   dynamics
///              f_tL = (aL f_l(lmL) f_v(vmL) + f_p(lmL)) cos(alphaL)
///              f_tR = (aR f_l(lmR) f_v(vmR) + f_p(lmR)) cos(alphaR)
///              x(0) = -0.03
///              y(0) = -d
///              vx(0) = 0
///              vy(0) = 0
///              aL(0) = 0
///              aR(0) = 0
///              x(0.5) = +0.03
///              y(0.5) = -d + 0.05
///              vx(0.5) = 0
///              vy(0.5) = 0
/// @endverbatim
template <typename T>
class OCPStatic : public mesh::OptimalControlProblemNamed<T> {
public:
    const double d = WIDTH;
    double mass = -1;
    int m_i_x = -1;
    int m_i_y = -1;
    int m_i_vx = -1;
    int m_i_vy = -1;
    int m_i_activation_l = -1;
    int m_i_activation_r = -1;
    DeGroote2016Muscle<T> m_muscleL;
    DeGroote2016Muscle<T> m_muscleR;
    struct NetForce {
        T x;
        T y;
    };

    OCPStatic(const Model& model) :
            mesh::OptimalControlProblemNamed<T>("2musc2dofstatic") {
        this->set_time(0, 0.2);
        m_i_x = this->add_state("x", {-0.03, 0.03}, -0.03, 0.03);
        m_i_y = this->add_state("y", {-2 * d, 0}, -d, -d + 0.05);
        m_i_vx = this->add_state("vx", {-15, 15}, 0, 0);
        m_i_vy = this->add_state("vy", {-15, 15}, 0, 0);
        m_i_activation_l = this->add_control("activation_l", {0, 1});
        m_i_activation_r = this->add_control("activation_r", {0, 1});
        mass = dynamic_cast<const Body&>(model.getComponent("body")).get_mass();
        {
            const auto& osimMuscleL =
                    dynamic_cast<const Muscle&>(model.getComponent("left"));
            m_muscleL = DeGroote2016Muscle<T>(
                    osimMuscleL.get_max_isometric_force(),
                    osimMuscleL.get_optimal_fiber_length(),
                    osimMuscleL.get_tendon_slack_length(),
                    osimMuscleL.get_pennation_angle_at_optimal(),
                    osimMuscleL.get_max_contraction_velocity());
        }
        {
            const auto& osimMuscleR =
                    dynamic_cast<const Muscle&>(model.getComponent("right"));
            m_muscleR = DeGroote2016Muscle<T>(
                    osimMuscleR.get_max_isometric_force(),
                    osimMuscleR.get_optimal_fiber_length(),
                    osimMuscleR.get_tendon_slack_length(),
                    osimMuscleR.get_pennation_angle_at_optimal(),
                    osimMuscleR.get_max_contraction_velocity());
        }
    }
    void dynamics(const mesh::VectorX<T>& states,
                  const mesh::VectorX<T>& controls,
                  Eigen::Ref<mesh::VectorX<T>> derivatives) const override {
        // Unpack variables.
        // -----------------
        const T& vx = states[m_i_vx];
        const T& vy = states[m_i_vy];

        // Multibody kinematics.
        // ---------------------
        derivatives[m_i_x] = vx;
        derivatives[m_i_y] = vy;

        // Multibody dynamics.
        // -------------------
        const auto netForce = calcNetForce(states, controls);
        derivatives[m_i_vx] = netForce.x / mass;
        derivatives[m_i_vy] = netForce.y / mass - ACCEL_GRAVITY;
    }
    NetForce calcNetForce(const mesh::VectorX<T>& states,
                          const mesh::VectorX<T>& controls) const {
        const T& x = states[m_i_x];
        const T& y = states[m_i_y];
        const T& vx = states[m_i_vx];
        const T& vy = states[m_i_vy];

        const T& activationL = controls[m_i_activation_l];
        const T musTenLenL = sqrt(pow(d + x, 2) + pow(y, 2));
        const T musTenVelL = ((d + x) * vx + y * vy) / musTenLenL;
        const T tensionL = m_muscleL.calcRigidTendonFiberForceAlongTendon(
                activationL, musTenLenL, musTenVelL);

        const T& activationR = controls[m_i_activation_r];
        const T musTenLenR = sqrt(pow(d - x, 2) + pow(y, 2));
        const T musTenVelR = (-(d - x) * vx + y * vy) / musTenLenR;
        const T tensionR = m_muscleR.calcRigidTendonFiberForceAlongTendon(
                activationR, musTenLenR, musTenVelR);

        const T netForceX = -tensionL * (d + x) / musTenLenL
                            +tensionR * (d - x) / musTenLenR;
        const T netForceY = +tensionL * (-y) / musTenLenL
                            +tensionR * (-y) / musTenLenR;
        return {netForceX,  netForceY};
    }
    void integral_cost(const T& /*time*/,
                       const mesh::VectorX<T>& /*states*/,
                       const mesh::VectorX<T>& controls,
                       T& integrand) const override {
        const auto& controlL = controls[m_i_activation_l];
        const auto& controlR = controls[m_i_activation_r];
        integrand = controlL * controlL + controlR * controlR;
    }
};

/// Move a point mass from a fixed starting state to a fixed end
/// position and velocity, in fixed time, with minimum effort. The point mass
/// has 2 DOFs (x and y translation).
///
///                            |< d >|< d >|
///                    ----------------------
///                             \         /
///                              .       .
///                   left muscle \     / right muscle
///                                .   .
///                                 \ /
///                                  O mass
///
/// Here's a sketch of the problem we solve, with activation and fiber dynamics.
/// @verbatim
///   minimize   int_t (aL^2 + aR^2) dt
///   subject to xdot = vx                                        kinematics
///              ydot = vy                                        kinematics
///              vxdot = 1/m (-f_tL (d+x)/lmtL + f_tR (d-x)/lmtR) dynamics
///              vydot = 1/m (-f_tL (-y)/lmtL + f_tR (-y)/lmtR)   dynamics
///              aLdot = f_a(eL, aL)       activation dynamics
///              aRdot = f_a(eR, aR)
///              lmLdot = vmLdot           fiber dynamics
///              lmRdot = vmRdot
///(for L and R) (a f_l(lm) f_v(vm) + f_p(lm)) cos(alpha) = f_t(lt) equilibrium
///              x(0) = -0.03
///              y(0) = -d
///              vx(0) = 0
///              vy(0) = 0
///              aL(0) = 0
///              aR(0) = 0
///              vmL(0) = 0
///              vmR(0) = 0
///              x(0.5) = +0.03
///              y(0.5) = -d + 0.05
///              vx(0.5) = 0
///              vy(0.5) = 0
/// @endverbatim
template <typename T>
class OCPDynamic : public mesh::OptimalControlProblemNamed<T> {
public:
    const double d = WIDTH;
    double mass = -1;
    int m_i_x = -1;
    int m_i_y = -1;
    int m_i_vx = -1;
    int m_i_vy = -1;
    int m_i_activation_l = -1;
    int m_i_activation_r = -1;
    int m_i_norm_fiber_length_l = -1;
    int m_i_norm_fiber_length_r = -1;
    int m_i_excitation_l = -1;
    int m_i_excitation_r = -1;
    int m_i_norm_fiber_velocity_l = -1;
    int m_i_fiber_equilibrium_l = -1;
    int m_i_norm_fiber_velocity_r = -1;
    int m_i_fiber_equilibrium_r = -1;
    DeGroote2016Muscle<T> m_muscleL;
    DeGroote2016Muscle<T> m_muscleR;
    struct NetForce {
        T x;
        T y;
    };
    OCPDynamic(const Model& model) :
            mesh::OptimalControlProblemNamed<T>("2musc2dofdynamic") {
        this->set_time(0, 0.5);
        m_i_x = this->add_state("x", {-0.03, 0.03}, -0.03, 0.03);
        m_i_y = this->add_state("y", {-2 * d, 0}, -d, -d + 0.05);
        m_i_vx = this->add_state("vx", {-15, 15}, 0, 0);
        m_i_vy = this->add_state("vy", {-15, 15}, 0, 0);
        m_i_activation_l = this->add_state("activation_l", {0, 1}, 0);
        m_i_activation_r = this->add_state("activation_r", {0, 1}, 0);
        m_i_norm_fiber_length_l =
                this->add_state("norm_fiber_length_l", {0.2, 1.8});
        m_i_norm_fiber_length_r =
                this->add_state("norm_fiber_length_r", {0.2, 1.8});
        m_i_excitation_l = this->add_control("excitation_l", {0, 1});
        m_i_excitation_r = this->add_control("excitation_r", {0, 1});
        m_i_norm_fiber_velocity_l =
                this->add_control("norm_fiber_velocity_l", {-1, 1}, 0);
        m_i_norm_fiber_velocity_r =
                this->add_control("norm_fiber_velocity_r", {-1, 1}, 0);
        m_i_fiber_equilibrium_l =
                this->add_path_constraint("fiber_equilibrium_l", 0);
        m_i_fiber_equilibrium_r =
                this->add_path_constraint("fiber_equilibrium_r", 0);
        mass = dynamic_cast<const Body&>(model.getComponent("body")).get_mass();
        {
            const auto& osimMuscleL =
                    dynamic_cast<const Muscle&>(model.getComponent("left"));
            m_muscleL = DeGroote2016Muscle<T>(
                    osimMuscleL.get_max_isometric_force(),
                    osimMuscleL.get_optimal_fiber_length(),
                    osimMuscleL.get_tendon_slack_length(),
                    osimMuscleL.get_pennation_angle_at_optimal(),
                    osimMuscleL.get_max_contraction_velocity());
        }
        {
            const auto& osimMuscleR =
                    dynamic_cast<const Muscle&>(model.getComponent("right"));
            m_muscleR = DeGroote2016Muscle<T>(
                    osimMuscleR.get_max_isometric_force(),
                    osimMuscleR.get_optimal_fiber_length(),
                    osimMuscleR.get_tendon_slack_length(),
                    osimMuscleR.get_pennation_angle_at_optimal(),
                    osimMuscleR.get_max_contraction_velocity());
        }
    }
    void dynamics(const mesh::VectorX<T>& states,
                  const mesh::VectorX<T>& controls,
                  Eigen::Ref<mesh::VectorX<T>> derivatives) const override {
        // Unpack variables.
        // -----------------
        const T& vx = states[m_i_vx];
        const T& vy = states[m_i_vy];

        // Multibody kinematics.
        // ---------------------
        derivatives[m_i_x] = vx;
        derivatives[m_i_y] = vy;

        // Multibody dynamics.
        // -------------------
        const auto netForce = calcNetForce(states);
        derivatives[m_i_vx] = netForce.x / mass;
        derivatives[m_i_vy] = netForce.y / mass - ACCEL_GRAVITY;

        // Activation dynamics.
        // --------------------
        const T& activationL = states[m_i_activation_l];
        const T& excitationL = controls[m_i_excitation_l];
        m_muscleL.calcActivationDynamics(excitationL, activationL,
                                         derivatives[m_i_activation_l]);
        const T& activationR = states[m_i_activation_r];
        const T& excitationR = controls[m_i_excitation_r];
        m_muscleR.calcActivationDynamics(excitationR, activationR,
                                         derivatives[m_i_activation_r]);

        // Fiber dynamics.
        // ---------------
        const T& normFibVelL = controls[m_i_norm_fiber_velocity_l];
        const T& normFibVelR = controls[m_i_norm_fiber_velocity_r];
        derivatives[m_i_norm_fiber_length_l] =
                m_muscleL.get_max_contraction_velocity() * normFibVelL;
        derivatives[m_i_norm_fiber_length_r] =
                m_muscleR.get_max_contraction_velocity() * normFibVelR;
    }
    NetForce calcNetForce(const mesh::VectorX<T>& states) const {
        const T& x = states[m_i_x];
        const T& y = states[m_i_y];

        T tensionL;
        const T musTenLenL = sqrt(pow(d + x, 2) + pow(y, 2));
        const T& normFibLenL = states[m_i_norm_fiber_length_l];
        m_muscleL.calcTendonForce(musTenLenL, normFibLenL, tensionL);

        T tensionR;
        const T musTenLenR = sqrt(pow(d - x, 2) + pow(y, 2));
        const T& normFibLenR = states[m_i_norm_fiber_length_r];
        m_muscleR.calcTendonForce(musTenLenR, normFibLenR, tensionR);

        const T netForceX = -tensionL * (d + x) / musTenLenL
                            +tensionR * (d - x) / musTenLenR;
        const T netForceY = +tensionL * (-y) / musTenLenL
                            +tensionR * (-y) / musTenLenR;
        return {netForceX,  netForceY};
    }
    void path_constraints(unsigned /*i_mesh*/,
                          const T& /*time*/,
                          const mesh::VectorX<T>& states,
                          const mesh::VectorX<T>& controls,
                          Eigen::Ref<mesh::VectorX<T>> constraints)
    const override {
        const T& x = states[m_i_x];
        const T& y = states[m_i_y];
        {
            const T& activationL = states[m_i_activation_l];
            const T& normFibLenL = states[m_i_norm_fiber_length_l];
            const T& normFibVelL = controls[m_i_norm_fiber_velocity_l];
            const T musTenLenL = sqrt(pow(d + x, 2) + pow(y, 2));
            m_muscleL.calcEquilibriumResidual(activationL, musTenLenL,
                    normFibLenL, normFibVelL,
                    constraints[m_i_fiber_equilibrium_l]);
        }
        {
            const T& activationR = states[m_i_activation_r];
            const T& normFibLenR = states[m_i_norm_fiber_length_r];
            const T& normFibVelR = controls[m_i_norm_fiber_velocity_r];
            const T musTenLenR = sqrt(pow(d - x, 2) + pow(y, 2));
            m_muscleR.calcEquilibriumResidual(activationR, musTenLenR,
                    normFibLenR, normFibVelR,
                    constraints[m_i_fiber_equilibrium_r]);
        }
    }
    void integral_cost(const T& /*time*/,
                       const mesh::VectorX<T>& /*states*/,
                       const mesh::VectorX<T>& controls,
                       T& integrand) const override {
        const auto& controlL = controls[m_i_excitation_l];
        const auto& controlR = controls[m_i_excitation_r];
        integrand = controlL * controlL + controlR * controlR;
    }
};

OpenSim::Model buildModel() {
    using SimTK::Vec3;

    Model model;
    // model.setUseVisualizer(true);
    model.setName("block2musc2dof");
    model.set_gravity(Vec3(0, -ACCEL_GRAVITY, 0));

    // Massless intermediate body.
    auto* intermed = new Body("intermed", 0, Vec3(0), SimTK::Inertia(0));
    model.addComponent(intermed);
    auto* body = new Body("body", 1, Vec3(0), SimTK::Inertia(1));
    model.addComponent(body);

    // TODO GSO and MRS do not support locked coordinates yet.
    // Allow translation along x and y; disable rotation about z.
    // auto* joint = new PlanarJoint();
    // joint->setName("joint");
    // joint->connectSocket_parent_frame(model.getGround());
    // joint->connectSocket_child_frame(*body);
    // auto& coordTX = joint->updCoordinate(PlanarJoint::Coord::TranslationX);
    // coordTX.setName("tx");
    // auto& coordTY = joint->updCoordinate(PlanarJoint::Coord::TranslationY);
    // coordTY.setName("ty");
    // auto& coordRZ = joint->updCoordinate(PlanarJoint::Coord::RotationZ);
    // coordRZ.setName("rz");
    // coordRZ.setDefaultLocked(true);
    // model.addComponent(joint);
    auto* jointX = new SliderJoint();
    jointX->setName("tx");
    jointX->connectSocket_parent_frame(model.getGround());
    jointX->connectSocket_child_frame(*intermed);
    auto& coordX = jointX->updCoordinate(SliderJoint::Coord::TranslationX);
    coordX.setName("tx");
    model.addComponent(jointX);

    // The joint's x axis must point in the global "+y" direction.
    auto* jointY = new SliderJoint("ty",
            *intermed, Vec3(0), Vec3(0, 0, 0.5 * SimTK::Pi),
            *body, Vec3(0), Vec3(0, 0, .5 * SimTK::Pi));
    auto& coordY = jointY->updCoordinate(SliderJoint::Coord::TranslationX);
    coordY.setName("ty");
    model.addComponent(jointY);

    {
        auto* actuL = new Millard2012EquilibriumMuscle();
        actuL->setName("left");
        actuL->set_max_isometric_force(40);
        actuL->set_optimal_fiber_length(.20);
        actuL->set_tendon_slack_length(0.10);
        actuL->set_pennation_angle_at_optimal(0.0);
        actuL->addNewPathPoint("origin", model.updGround(),
                               Vec3(-WIDTH, 0, 0));
        actuL->addNewPathPoint("insertion", *body, Vec3(0));
        model.addComponent(actuL);
    }
    {
        auto* actuR = new Millard2012EquilibriumMuscle();
        actuR->setName("right");
        actuR->set_max_isometric_force(40);
        actuR->set_optimal_fiber_length(.21);
        actuR->set_tendon_slack_length(0.09);
        actuR->set_pennation_angle_at_optimal(0.0);
        actuR->addNewPathPoint("origin", model.updGround(),
                               Vec3(+WIDTH, 0, 0));
        actuR->addNewPathPoint("insertion", *body, Vec3(0));
        model.addComponent(actuR);
    }

    // For use in "filebased" tests.
    model.print("test2Muscles2DOFsDeGroote2016.osim");
    // SimTK::State s = model.initSystem();
    // model.equilibrateMuscles(s);
    // model.updMatterSubsystem().setShowDefaultGeometry(true);
    // model.updVisualizer().updSimbodyVisualizer().setBackgroundType(
    //         SimTK::Visualizer::GroundAndSky);
    // model.getVisualizer().show(s);
    // std::cin.get();
    // Manager manager(model);
    // manager.integrate(s, 1.0);
    // std::cin.get();
    return model;
}

std::pair<TimeSeriesTable, TimeSeriesTable>
solveForTrajectory_GSO(const Model& model) {

    // Solve a trajectory optimization problem.
    auto ocp = std::make_shared<OCPStatic<adouble>>(model);
    ocp->print_description();
    const int N = 100;
    mesh::DirectCollocationSolver<adouble> dircol(ocp, "trapezoidal",
                                                  "ipopt", N);
    mesh::OptimalControlSolution ocp_solution = dircol.solve();
    std::string trajectoryFile =
            "test2Muscles2DOFsDeGroote2016_GSO_trajectory.csv";
    ocp_solution.write(trajectoryFile);

    // Save the trajectory with a header so that OpenSim can read it.
    // --------------------------------------------------------------
    // CSVFileAdapter expects an "endheader" line in the file.
    auto fRead = std::ifstream(trajectoryFile);
    std::string trajFileWithHeader = trajectoryFile;
    trajFileWithHeader.replace(trajectoryFile.rfind(".csv"), 4,
                               "_with_header.csv");
    // Skip the "num_states=#" and "num_controls=#" lines.
    std::string line;
    std::getline(fRead, line);
    std::getline(fRead, line);
    auto fWrite = std::ofstream(trajFileWithHeader);
    fWrite << "endheader" << std::endl;
    while (std::getline(fRead, line)) fWrite << line << std::endl;
    fRead.close();
    fWrite.close();

    // Create a table containing only the angle and speed of the pendulum.
    TimeSeriesTable ocpSolution = CSVFileAdapter::read(trajFileWithHeader);
    TimeSeriesTable kinematics;
    kinematics.setColumnLabels(
            {"tx/tx/value", "tx/tx/speed", "ty/ty/value", "ty/ty/speed"});
    const auto& x = ocpSolution.getDependentColumn("x");
    const auto& vx = ocpSolution.getDependentColumn("vx");
    const auto& y = ocpSolution.getDependentColumn("y");
    const auto& vy = ocpSolution.getDependentColumn("vy");
    SimTK::RowVector row(4);
    for (size_t iRow = 0; iRow < ocpSolution.getNumRows(); ++iRow) {
        row[0] = x[iRow];
        row[1] = vx[iRow];
        row[2] = y[iRow];
        row[3] = vy[iRow];
        kinematics.appendRow(ocpSolution.getIndependentColumn()[iRow], row);
    }
    // For use in the "filebased" test.
    CSVFileAdapter::write(kinematics,
            "test2Muscles2DOFsDeGroote2016_GSO_kinematics.csv");

    // Compute actual inverse dynamics moment, for debugging.
    // ------------------------------------------------------
    TimeSeriesTable actualInvDyn;
    actualInvDyn.setColumnLabels({"x", "y"});
    auto ocpd = std::make_shared<OCPStatic<double>>(model);
    for (Eigen::Index iTime = 0; iTime < ocp_solution.time.size(); ++iTime) {
        auto netForce = ocpd->calcNetForce(ocp_solution.states.col(iTime),
                                           ocp_solution.controls.col(iTime));
        SimTK::RowVector row(2);
        row[0] = netForce.x;
        row[1] = netForce.y;
        actualInvDyn.appendRow(ocp_solution.time(iTime), row);
    }
    CSVFileAdapter::write(actualInvDyn,
                          "DEBUG_test2Muscles2DOFs_GSO_actualInvDyn.csv");

    return {ocpSolution, kinematics};
}

std::pair<TimeSeriesTable, TimeSeriesTable>
solveForTrajectory_MRS(const Model& model) {
    // Solve a trajectory optimization problem.
    // ----------------------------------------
    auto ocp = std::make_shared<OCPDynamic<adouble>>(model);
    ocp->print_description();
    const int N = 100;
    mesh::DirectCollocationSolver<adouble> dircol(ocp, "trapezoidal",
                                                  "ipopt", N);

    mesh::OptimalControlIterate guess(
            "test2Muscles2DOFsDeGroote2016_MRS_initial_guess.csv");
    mesh::OptimalControlSolution ocp_solution = dircol.solve(guess);
    //mesh::OptimalControlSolution ocp_solution = dircol.solve();
    dircol.print_constraint_values(ocp_solution);

    std::string trajectoryFile =
            "test2Muscles2DOFsDeGroote2016_MRS_trajectory.csv";
    ocp_solution.write(trajectoryFile);

    // Save the trajectory with a header so that OpenSim can read it.
    // --------------------------------------------------------------
    // CSVFileAdapter expects an "endheader" line in the file.
    auto fRead = std::ifstream(trajectoryFile);
    std::string trajFileWithHeader = trajectoryFile;
    trajFileWithHeader.replace(trajectoryFile.rfind(".csv"), 4,
                               "_with_header.csv");
    // Skip the "num_states=#" and "num_controls=#" lines.
    std::string line;
    std::getline(fRead, line);
    std::getline(fRead, line);
    auto fWrite = std::ofstream(trajFileWithHeader);
    fWrite << "endheader" << std::endl;
    while (std::getline(fRead, line)) fWrite << line << std::endl;
    fRead.close();
    fWrite.close();

    // Create a table containing only the angle and speed of the pendulum.
    TimeSeriesTable ocpSolution = CSVFileAdapter::read(trajFileWithHeader);
    TimeSeriesTable kinematics;
    kinematics.setColumnLabels(
            {"tx/tx/value", "tx/tx/speed", "ty/ty/value", "ty/ty/speed"});
    const auto& x = ocpSolution.getDependentColumn("x");
    const auto& vx = ocpSolution.getDependentColumn("vx");
    const auto& y = ocpSolution.getDependentColumn("y");
    const auto& vy = ocpSolution.getDependentColumn("vy");
    SimTK::RowVector row(4);
    for (size_t iRow = 0; iRow < ocpSolution.getNumRows(); ++iRow) {
        row[0] = x[iRow];
        row[1] = vx[iRow];
        row[2] = y[iRow];
        row[3] = vy[iRow];
        kinematics.appendRow(ocpSolution.getIndependentColumn()[iRow], row);
    }
    // For use in the "filebased" test.
    CSVFileAdapter::write(kinematics,
            "test2Muscles2DOFsDeGroote2016_MRS_kinematics.csv");

    // Compute actual inverse dynamics moment, for debugging.
    // ------------------------------------------------------
    TimeSeriesTable actualInvDyn;
    actualInvDyn.setColumnLabels({"x", "y"});
    auto ocpd = std::make_shared<OCPDynamic<double>>(model);
    for (Eigen::Index iTime = 0; iTime < ocp_solution.time.size(); ++iTime) {
        auto netForce = ocpd->calcNetForce(ocp_solution.states.col(iTime));
        SimTK::RowVector row(2);
        row[0] = netForce.x;
        row[1] = netForce.y;
        actualInvDyn.appendRow(ocp_solution.time(iTime), row);
    }
    CSVFileAdapter::write(actualInvDyn,
                          "DEBUG_test2Muscles2DOFs_MRS_actualInvDyn.csv");
    return {ocpSolution, kinematics};
}

void compareSolution_GSO(const GlobalStaticOptimizationSolver::Solution& actual,
                         const TimeSeriesTable& expected,
                         const double& reserveOptimalForce) {
    compare(actual.activation, "/block2musc2dof/left",
            expected,          "activation_l",
            0.01);
    compare(actual.activation, "/block2musc2dof/right",
            expected,          "activation_r",
            0.05);
    auto reserveForceXRMS = reserveOptimalForce *
            actual.other_controls.getDependentColumnAtIndex(0).normRMS();
    SimTK_TEST(reserveForceXRMS < 0.01);
    auto reserveForceYRMS = reserveOptimalForce *
            actual.other_controls.getDependentColumnAtIndex(1).normRMS();
    SimTK_TEST(reserveForceYRMS < 0.01);
}

void test2Muscles2DOFs_GSO(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data,
        const Model& model) {
    const auto& ocpSolution = data.first;
    const auto& kinematics = data.second;

    GlobalStaticOptimizationSolver gso;
    gso.setModel(model);
    gso.setKinematicsData(kinematics);
    gso.set_lowpass_cutoff_frequency_for_joint_moments(80);
    double reserveOptimalForce = 0.001;
    gso.set_create_reserve_actuators(reserveOptimalForce);
    // gso.print("test2Muscles2DOFsDeGroote2016_GSO_setup.xml");
    GlobalStaticOptimizationSolver::Solution solution = gso.solve();
    // solution.write("test2Muscles2DOFsDeGroote2016_GSO");

    // Compare the solution to the initial trajectory optimization solution.
    compareSolution_GSO(solution, ocpSolution, reserveOptimalForce);
}

// Load all settings from a setup file, and run the same tests as in the test
// above.
void test2Muscles2DOFs_GSO_Filebased(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data) {
    const auto& ocpSolution = data.first;

    GlobalStaticOptimizationSolver gso(
            "test2Muscles2DOFsDeGroote2016_GSO_setup.xml");
    double reserveOptimalForce = gso.get_create_reserve_actuators();
    GlobalStaticOptimizationSolver::Solution solution = gso.solve();

    // Compare the solution to the initial trajectory optimization solution.
    compareSolution_GSO(solution, ocpSolution, reserveOptimalForce);
}

void test2Muscles2DOFs_GSO_GenForces(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data,
        const Model& model) {
    const auto& ocpSolution = data.first;
    const auto& kinematics = data.second;

    // Run inverse dynamics.
    Model modelForID = model;
    modelForID.initSystem();
    InverseMuscleSolverMotionData motionData(modelForID, kinematics, 80,
            0, 0.2);
    Eigen::VectorXd times = Eigen::VectorXd::LinSpaced(100, 0, 0.2);
    Eigen::MatrixXd netGenForcesEigen;
    motionData.interpolateNetGeneralizedForces(times, netGenForcesEigen);
    // Convert Eigen Matrix to TimeSeriesTable.
    // This constructor expects a row-major matrix layout, but the Eigen matrix
    // is column-major. We can exploit this to transpose the data from
    // "DOFs x time" to "time x DOFs".
    SimTK::Matrix netGenForcesMatrix(
            netGenForcesEigen.cols(), netGenForcesEigen.rows(),
            netGenForcesEigen.data());
    TimeSeriesTable netGenForces;
    netGenForces.setColumnLabels({"tx/tx", "ty/ty"});
    for (int iRow = 0; iRow < netGenForcesMatrix.nrow(); ++iRow) {
        netGenForces.appendRow(times[iRow], netGenForcesMatrix.row(iRow));
    }

    GlobalStaticOptimizationSolver gso;
    gso.setModel(model);
    gso.setKinematicsData(kinematics);
    gso.setNetGeneralizedForcesData(netGenForces);
    double reserveOptimalForce = 0.001;
    gso.set_create_reserve_actuators(reserveOptimalForce);
    GlobalStaticOptimizationSolver::Solution solution = gso.solve();

    // Compare the solution to the initial trajectory optimization solution.
    compareSolution_GSO(solution, ocpSolution, reserveOptimalForce);
}

void compareSolution_MRS(const MuscleRedundancySolver::Solution& actual,
                         const TimeSeriesTable& expected,
                         const double& reserveOptimalForce) {
    compare(actual.activation, "/block2musc2dof/left",
            expected,          "activation_l",
            0.05);
    compare(actual.activation, "/block2musc2dof/right",
            expected,          "activation_r",
            0.05);

    compare(actual.norm_fiber_length, "/block2musc2dof/left",
            expected,                 "norm_fiber_length_l",
            0.01);
    compare(actual.norm_fiber_length, "/block2musc2dof/right",
            expected,                 "norm_fiber_length_r",
            0.01);

    // We use a weaker check for the controls; they don't match as well.
    // The excitations are fairly noisy across time, and the fiber velocity
    // does not match well at the beginning of the trajectory.
    rootMeanSquare(actual.excitation, "/block2musc2dof/left",
            expected,                 "excitation_l",
            0.03);
    rootMeanSquare(actual.excitation, "/block2musc2dof/right",
            expected,                 "excitation_r",
            0.03);

    rootMeanSquare(actual.norm_fiber_velocity, "/block2musc2dof/left",
            expected,                          "norm_fiber_velocity_l",
            0.02);
    rootMeanSquare(actual.norm_fiber_velocity, "/block2musc2dof/right",
            expected,                          "norm_fiber_velocity_r",
            0.02);

    auto reserveForceXRMS = reserveOptimalForce *
            actual.other_controls.getDependentColumnAtIndex(0).normRMS();
    SimTK_TEST(reserveForceXRMS < 0.05);
    auto reserveForceYRMS = reserveOptimalForce *
            actual.other_controls.getDependentColumnAtIndex(1).normRMS();
    SimTK_TEST(reserveForceYRMS < 0.15);
}

// Reproduce the trajectory (generated with muscle dynamics) using the
// MuscleRedundancySolver.
void test2Muscles2DOFs_MRS(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data,
        const Model& model) {
    const auto& ocpSolution = data.first;
    const auto& kinematics = data.second;

    // Create the MuscleRedundancySolver.
    // ----------------------------------
    MuscleRedundancySolver mrs;
    mrs.setModel(model);
    mrs.setKinematicsData(kinematics);
    mrs.set_lowpass_cutoff_frequency_for_joint_moments(20);
    const double reserveOptimalForce = 0.01;
    mrs.set_create_reserve_actuators(reserveOptimalForce);
    // We constrain initial MRS activation to 0 because otherwise activation
    // incorrectly starts at a large value (no penalty for large initial
    // activation).
    mrs.set_zero_initial_activation(true);
    // mrs.print("test2Muscles2DOFsDeGroote2016_MRS_setup.xml");
    MuscleRedundancySolver::Solution solution = mrs.solve();
    // solution.write("test2Muscles2DOFsDeGroote2016_MRS");

    // Compare the solution to the initial trajectory optimization solution.
    // ---------------------------------------------------------------------
    compareSolution_MRS(solution, ocpSolution, reserveOptimalForce);
}

// Load MRS from an XML file.
void test2Muscles2DOFs_MRS_Filebased(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data) {
    const auto& ocpSolution = data.first;

    // Create the MuscleRedundancySolver.
    MuscleRedundancySolver mrs("test2Muscles2DOFsDeGroote2016_MRS_setup.xml");
    double reserveOptimalForce = mrs.get_create_reserve_actuators();
    MuscleRedundancySolver::Solution solution = mrs.solve();

    // Compare the solution to the initial trajectory optimization solution.
    compareSolution_MRS(solution, ocpSolution, reserveOptimalForce);
}

// Perform inverse dynamics outside of MRS.
void test2Muscles2DOFs_MRS_GenForces(
        const std::pair<TimeSeriesTable, TimeSeriesTable>& data,
        const Model& model) {
    const auto& ocpSolution = data.first;
    const auto& kinematics = data.second;

    // Run inverse dynamics.
    // ---------------------
    // This constructor performs inverse dynamics:
    Model modelForID = model;
    modelForID.initSystem();
    InverseMuscleSolverMotionData motionData(modelForID, kinematics, 20,
            0, 0.5);
    Eigen::VectorXd times = Eigen::VectorXd::LinSpaced(100, 0, 0.5);
    Eigen::MatrixXd netGenForcesEigen;
    motionData.interpolateNetGeneralizedForces(times, netGenForcesEigen);
    // Convert Eigen Matrix to TimeSeriesTable.
    // This constructor expects a row-major matrix layout, but the Eigen matrix
    // is column-major. We can exploit this to transpose the data from
    // "DOFs x time" to "time x DOFs".
    SimTK::Matrix netGenForcesMatrix(
            netGenForcesEigen.cols(), netGenForcesEigen.rows(),
            netGenForcesEigen.data());
    TimeSeriesTable netGenForces;
    netGenForces.setColumnLabels({"tx/tx", "ty/ty"});
    for (int iRow = 0; iRow < netGenForcesMatrix.nrow(); ++iRow) {
        netGenForces.appendRow(times[iRow], netGenForcesMatrix.row(iRow));
    }

    // Create the MuscleRedundancySolver.
    // ----------------------------------
    MuscleRedundancySolver mrs;
    mrs.setModel(model);
    mrs.setKinematicsData(kinematics);
    const double reserveOptimalForce = 0.01;
    mrs.set_create_reserve_actuators(reserveOptimalForce);
    mrs.set_zero_initial_activation(true);
    mrs.setNetGeneralizedForcesData(netGenForces);
    MuscleRedundancySolver::Solution solution = mrs.solve();

    // Compare the solution to the initial trajectory optimization solution.
    // ---------------------------------------------------------------------
    compareSolution_MRS(solution, ocpSolution, reserveOptimalForce);
}

int main() {
    SimTK_START_TEST("test2Muscles2DOFsDeGroote2016");
        Model model = buildModel();
        model.finalizeFromProperties();
        {
            auto data = solveForTrajectory_GSO(model);
            SimTK_SUBTEST2(test2Muscles2DOFs_GSO, data, model);
            SimTK_SUBTEST1(test2Muscles2DOFs_GSO_Filebased, data);
            SimTK_SUBTEST2(test2Muscles2DOFs_GSO_GenForces, data, model);
        }
        {
            auto data = solveForTrajectory_MRS(model);
            SimTK_SUBTEST2(test2Muscles2DOFs_MRS, data, model);
            SimTK_SUBTEST1(test2Muscles2DOFs_MRS_Filebased, data);
            SimTK_SUBTEST2(test2Muscles2DOFs_MRS_GenForces, data, model);
        }
    SimTK_END_TEST();
}