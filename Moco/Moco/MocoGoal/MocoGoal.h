#ifndef MOCO_MOCOGOAL_H
#define MOCO_MOCOGOAL_H
/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoGoal.h                                                   *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Christopher Dembia                                              *
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

#include "../MocoBounds.h"
#include "../MocoConstraintInfo.h"
#include "../osimMocoDLL.h"

#include <SimTKcommon/internal/State.h>

#include <OpenSim/Common/Object.h>

namespace OpenSim {

class Model;

// TODO give option to specify gradient and Hessian analytically.

/// A term in the cost functional, to be minimized. Costs depend on the phase's
/// initial and final states and controls, and optionally on the integral of a
/// quantity over the phase.
/// TODO document that this can also be an endpoint constraint.
/// @par For developers
/// Every time the problem is solved, a copy of this cost is used. An individual
/// instance of a cost is only ever used in a single problem. Therefore, there
/// is no need to clear cache variables that you create in initializeImpl().
/// Also, information stored in this cost does not persist across multiple
/// solves.
/// @ingroup mocogoal
class OSIMMOCO_API MocoGoal : public Object {
    OpenSim_DECLARE_ABSTRACT_OBJECT(MocoGoal, Object);

public:
    OpenSim_DECLARE_PROPERTY(
            enabled, bool, "This bool indicates whether this cost is enabled.");

    OpenSim_DECLARE_PROPERTY(weight, double,
            "The cost value is multiplied by this weight (default: 1).");

    // TODO rename to "mode", which is either "cost" or "endpoint_constraint"
    OpenSim_DECLARE_OPTIONAL_PROPERTY(
            apply_as_endpoint_constraint, bool, "TODO");

    MocoGoal();

    MocoGoal(std::string name);

    MocoGoal(std::string name, double weight);

    // TODO: an Endpoint should have a default as to whether it's applied as a
    // cost or a constraint.
    bool getSupportsEndpointConstraint() const {
        return getSupportsEndpointConstraintImpl();
    }

    bool getDefaultEndpointConstraint() const {
        return getDefaultEndpointConstraintImpl();
    }

    const MocoConstraintInfo& getConstraintInfo() const {
        // TODO hack to setNumEquations().
        getNumOutputs();
        return get_MocoConstraintInfo();
    }

    // TODO: the issue with relying on constraintinfo for num outputs is that
    // we don't use constraint info when applying as cost.
    int getNumOutputs() const {
        int n = getNumOutputsImpl();
        // TODO this is a bit hacky.
        setNumEquations(n);
        return n;
    }

    // TODO rename getApplyAsConstraint(). or getApplyAsEndpointConstraint().
    bool getApplyAsEndpointConstraint() const {
        bool endpoint_constraint;
        if (getProperty_apply_as_endpoint_constraint().empty()) {
            endpoint_constraint = getDefaultEndpointConstraint();
        } else {
            endpoint_constraint = get_apply_as_endpoint_constraint();
        }
        OPENSIM_THROW_IF(
                endpoint_constraint && !getSupportsEndpointConstraint(),
                Exception, "Endpoint constraint not supported TODO.");
        return endpoint_constraint;
    }

    /// Get the number of integrals required by this cost.
    /// This returns either 0 (for a strictly-endpoint cost) or 1.
    int getNumIntegrals() const {
        int num = getNumIntegralsImpl();
        OPENSIM_THROW_IF(num < 0, Exception,
                "Number of integrals must be non-negative.");
        return num;
    }
    /// Calculate the integrand that should be integrated and passed to
    /// calcCost(). If getNumIntegrals() is not zero, this must be implemented.
    SimTK::Real calcIntegrand(const SimTK::State& state) const {
        double integrand = 0;
        if (!get_enabled()) { return integrand; }
        calcIntegrandImpl(state, integrand);
        return integrand;
    }
    struct GoalInput {
        const SimTK::State& initial_state;
        const SimTK::State& final_state;
        /// This is computed by integrating calcIntegrand().
        const double& integral;
    };
    // TODO: rename to calcValue().
    /// The returned cost includes the weight.
    // We use SimTK::Real instead of double for when we support adoubles.
    SimTK::Vector calcGoal(const GoalInput& input) const {
        SimTK::Vector goal(getNumOutputs());
        goal = 0;
        if (!get_enabled()) { return goal; }
        calcGoalImpl(input, goal);
        if (!getApplyAsEndpointConstraint()) {
            goal *= get_weight();
        }
        return goal;
    }
    /// For use by solvers. This also performs error checks on the Problem.
    void initializeOnModel(const Model& model) const {
        m_model.reset(&model);
        if (!get_enabled()) { return; }
        // TODO hack to setNumEquations().
        // TODO setNumEquations inside initializeOnModel().
        setNumEquations(getNumOutputs());
        initializeOnModelImpl(model);
        // TODO check numEquations to ensure it's set.
    }

    /// Print the name, type, and weight for this cost.
    void printDescription(std::ostream& stream = std::cout) const;

protected:
    /// Perform any caching before the problem is solved.
    /// @precondition The model is initialized (initSystem()) and getModel()
    /// is available.
    /// The passed-in model is equivalent to getModel().
    /// Use this opportunity to check for errors in user input.
    // TODO: Rename to extendInitializeOnModel().
    virtual void initializeOnModelImpl(const Model&) const {}

    virtual int getNumOutputsImpl() const { return 1; }
    virtual bool getDefaultEndpointConstraintImpl() const { return false; }
    virtual bool getSupportsEndpointConstraintImpl() const { return false; }
    /// Return the number if integral terms required by this cost.
    /// This must be either 0 or 1.
    virtual int getNumIntegralsImpl() const = 0;
    /// @precondition The state is realized to SimTK::Stage::Position.
    /// If you need access to the controls, you must realize to Velocity:
    /// @code
    /// getModel().realizeVelocity(state);
    /// @endcode
    /// The Lagrange multipliers for kinematic constraints are not available.
    virtual void calcIntegrandImpl(
            const SimTK::State& state, double& integrand) const;
    /// The Lagrange multipliers for kinematic constraints are not available.
    virtual void calcGoalImpl(
            const GoalInput& input, SimTK::Vector& cost) const = 0;
    /// For use within virtual function implementations.
    const Model& getModel() const {
        OPENSIM_THROW_IF(!m_model, Exception,
                "Model is not available until the start of initializing.");
        return m_model.getRef();
    }

private:
    OpenSim_DECLARE_UNNAMED_PROPERTY(MocoConstraintInfo,
            "The bounds and labels for this MocoGoal, if applied as an "
            "endpoint constraint.");

    void setNumEquations(int numEqs) const {
        // TODO avoid const_cast
        const_cast<MocoGoal*>(this)->upd_MocoConstraintInfo().setNumEquations(
                numEqs);
    }

    void constructProperties();

    mutable SimTK::ReferencePtr<const Model> m_model;
};

inline void MocoGoal::calcIntegrandImpl(
        const SimTK::State& state, double& integrand) const {}

/// Endpoint cost for final time.
/// @ingroup mocogoal
class OSIMMOCO_API MocoFinalTimeGoal : public MocoGoal {
    OpenSim_DECLARE_CONCRETE_OBJECT(MocoFinalTimeGoal, MocoGoal);

public:
    MocoFinalTimeGoal() = default;
    MocoFinalTimeGoal(std::string name) : MocoGoal(std::move(name)) {}
    MocoFinalTimeGoal(std::string name, double weight)
            : MocoGoal(std::move(name), weight) {}

protected:
    int getNumIntegralsImpl() const override { return 0; }
    void calcGoalImpl(
            const GoalInput& input, SimTK::Vector& cost) const override {
        cost[0] = input.final_state.getTime();
    }
};

} // namespace OpenSim

#endif // MOCO_MOCOGOAL_H