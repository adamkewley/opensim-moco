#ifndef MOCO_MOCOCONTROLCONSTRAINT_H
#define MOCO_MOCOCONTROLCONSTRAINT_H
/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoControlConstraint.h                                      *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2019 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Nicholas Bianco                                                 *
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

#include "MocoConstraint.h"

#include <OpenSim/Common/TimeSeriesTable.h>
#include <OpenSim/Common/GCVSplineSet.h>

namespace OpenSim {

class OSIMMOCO_API MocoControlConstraint : public MocoPathConstraint {
    OpenSim_DECLARE_CONCRETE_OBJECT(MocoControlConstraint, MocoPathConstraint);

public:
    MocoControlConstraint() {}

    void setReference(const TimeSeriesTable& ref) {
        m_table = ref;
    }

protected:
    void initializeOnModelImpl(const Model& model) const override;
    void calcPathConstraintErrorsImpl(const SimTK::State& state,
        SimTK::Vector& errors) const override;

private:
    TimeSeriesTable m_table;
    mutable GCVSplineSet m_refsplines;
    mutable std::vector<int> m_controlIndices;

};

} // namespace OpenSim

#endif // MOCO_MOCOCONTROLCONSTRAINT_H