/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoUtilities.cpp                                            *
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

#include "MocoUtilities.h"

#include "MocoIterate.h"
#include <cstdarg>
#include <cstdio>

#include <simbody/internal/Visualizer_InputListener.h>

#include <OpenSim/Actuators/CoordinateActuator.h>
#include <OpenSim/Common/GCVSpline.h>
#include <OpenSim/Common/PiecewiseLinearFunction.h>
#include <OpenSim/Common/TimeSeriesTable.h>
#include <OpenSim/Simulation/Control/PrescribedController.h>
#include <OpenSim/Simulation/Manager/Manager.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/SimbodyEngine/WeldJoint.h>
#include <OpenSim/Simulation/StatesTrajectory.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <OpenSim/Simulation/StatesTrajectoryReporter.h>

using namespace OpenSim;

SimTK::Vector OpenSim::createVectorLinspace(
        int length, double start, double end) {
    SimTK::Vector v(length);
    for (int i = 0; i < length; ++i) {
        v[i] = start + i * (end - start) / (length - 1);
    }
    return v;
}

SimTK::Vector OpenSim::createVector(
        std::initializer_list<SimTK::Real> elements) {
    return SimTK::Vector((int)elements.size(), elements.begin());
}

SimTK::Vector OpenSim::interpolate(const SimTK::Vector& x,
        const SimTK::Vector& y, const SimTK::Vector& newX,
        const bool ignoreNaNs) {

    // Create vectors of non-NaN values if user set 'ignoreNaNs' argument to
    // 'true', otherwise throw an exception. If no NaN's are present in the
    // provided data vectors, the '*_no_nans' variables below will contain
    // the original data vector values.
    std::vector<double> x_no_nans;
    std::vector<double> y_no_nans;
    for (int i = 0; i < x.size(); ++i) {

        bool shouldNotPushBack =
                (SimTK::isNaN(x[i]) || SimTK::isNaN(y[i])) && ignoreNaNs;
        if (!shouldNotPushBack) {
            x_no_nans.push_back(x[i]);
            y_no_nans.push_back(y[i]);
        }
    }

    PiecewiseLinearFunction function(
            (int)x_no_nans.size(), &x_no_nans[0], &y_no_nans[0]);
    SimTK::Vector newY(newX.size(), SimTK::NaN);
    for (int i = 0; i < newX.size(); ++i) {
        const auto& newXi = newX[i];
        if (x_no_nans[0] <= newXi && newXi <= x_no_nans[x_no_nans.size() - 1])
            newY[i] = function.calcValue(SimTK::Vector(1, newXi));
    }
    return newY;
}

Storage OpenSim::convertTableToStorage(const TimeSeriesTable& table) {

    Storage sto;
    if (table.hasTableMetaDataKey("inDegrees") &&
            table.getTableMetaDataAsString("inDegrees") == "yes") {
        sto.setInDegrees(true);
    }

    OpenSim::Array<std::string> labels("", (int)table.getNumColumns() + 1);
    labels[0] = "time";
    for (int i = 0; i < (int)table.getNumColumns(); ++i) {
        labels[i + 1] = table.getColumnLabel(i);
    }
    sto.setColumnLabels(labels);
    const auto& times = table.getIndependentColumn();
    for (unsigned i_time = 0; i_time < table.getNumRows(); ++i_time) {
        SimTK::Vector row(table.getRowAtIndex(i_time).transpose());
        // This is a hack to allow creating a Storage with 0 columns.
        double unused;
        sto.append(times[i_time], row.size(),
                row.size() ? row.getContiguousScalarData() : &unused);
    }
    return sto;
}

TimeSeriesTable OpenSim::filterLowpass(
        const TimeSeriesTable& table, double cutoffFreq, bool padData) {
    OPENSIM_THROW_IF(cutoffFreq < 0, Exception,
            format("Cutoff frequency must be non-negative; got %g.",
                    cutoffFreq));
    auto storage = convertTableToStorage(table);
    if (padData) { storage.pad(storage.getSize() / 2); }
    storage.lowpassIIR(cutoffFreq);

    return storage.exportToTable();
}

// Based on code from simtk.org/projects/predictivesim SimbiconExample/main.cpp.
void OpenSim::visualize(Model model, Storage statesSto) {

    const SimTK::Real initialTime = statesSto.getFirstTime();
    const SimTK::Real finalTime = statesSto.getLastTime();
    const SimTK::Real duration = finalTime - initialTime;

    // A data rate of 300 Hz means we can maintain 30 fps down to
    // realTimeScale = 0.1. But if we have more than 20 seconds of data, then
    // we lower the data rate to avoid using too much memory.
    const double desiredNumStates = std::min(300 * duration, 300.0 * 20.0);
    const double dataRate = desiredNumStates / duration; // Hz
    const double frameRate = 30;                         // Hz.

    // Prepare data.
    // -------------
    statesSto.resample(1.0 / dataRate, 4 /* degree */);
    auto statesTraj = StatesTrajectory::createFromStatesStorage(
            model, statesSto, true, true, false);
    const int numStates = (int)statesTraj.getSize();

    // Must setUseVisualizer() *after* createFromStatesStorage(), otherwise
    // createFromStatesStorage() spawns a visualizer.
    model.setUseVisualizer(true);
    model.initSystem();

    // OPENSIM_THROW_IF(!statesTraj.isCompatibleWith(model), Exception,
    //        "Model is not compatible with the provided StatesTrajectory.");

    // Set up visualization.
    // ---------------------
    // model.updMatterSubsystem().setShowDefaultGeometry(true);
    auto& viz = model.updVisualizer().updSimbodyVisualizer();
    std::string modelName =
            model.getName().empty() ? "<unnamed>" : model.getName();
    std::string title = "Visualizing model '" + modelName + "'";
    if (!statesSto.getName().empty() && statesSto.getName() != "UNKNOWN")
        title += " with motion '" + statesSto.getName() + "'";
    {
        using namespace std::chrono;
        auto time_now = system_clock::to_time_t(system_clock::now());
        std::stringstream ss;
        // ISO standard extended datetime format.
        // https://kjellkod.wordpress.com/2013/01/22/exploring-c11-part-2-localtime-and-time-again/
        struct tm buf;
#if defined(_WIN32)
        localtime_s(&buf, &time_now);
#else
        localtime_r(&time_now, &buf);
#endif
        ss << std::put_time(&buf, "%Y-%m-%dT%X");
        title += " (" + ss.str() + ")";
    }
    viz.setWindowTitle(title);
    viz.setMode(SimTK::Visualizer::RealTime);
    // Buffering causes issues when the user adjusts the "Speed" slider.
    viz.setDesiredBufferLengthInSec(0);
    viz.setDesiredFrameRate(frameRate);
    viz.setShowSimTime(true);
    // viz.setBackgroundType(viz.SolidColor);
    // viz.setBackgroundColor(SimTK::White);
    // viz.setShowFrameRate(true);
    // viz.setShowFrameNumber(true);
    auto& silo = model.updVisualizer().updInputSilo();

    // BodyWatcher to control camera.
    // TODO

    // Add sliders to control playback.
    // Real-time factor:
    //      1 means simulation-time = real-time
    //      2 means playback is 2x faster.
    const int realTimeScaleSliderIndex = 1;
    const double minRealTimeScale = 0.01; // can't go to 0.
    const double maxRealTimeScale = 4;
    double realTimeScale = 1.0;
    viz.addSlider("Speed", realTimeScaleSliderIndex, minRealTimeScale,
            maxRealTimeScale, realTimeScale);

    // TODO this slider results in choppy playback if not paused.
    const int timeSliderIndex = 2;
    double time = initialTime;
    viz.addSlider("Time", timeSliderIndex, initialTime, finalTime, time);

    SimTK::Array_<std::pair<SimTK::String, int>> keyBindingsMenu;
    keyBindingsMenu.push_back(std::make_pair(
            "Available key bindings (clicking these menu items has no effect):",
            1));
    keyBindingsMenu.push_back(std::make_pair(
            "-----------------------------------------------------------------",
            2));
    keyBindingsMenu.push_back(std::make_pair("Pause: Space", 3));
    keyBindingsMenu.push_back(std::make_pair("Zoom to fit: R", 4));
    keyBindingsMenu.push_back(std::make_pair("Quit: Esc", 5));
    viz.addMenu("Key bindings", 1, keyBindingsMenu);

    SimTK::DecorativeText pausedText("");
    pausedText.setIsScreenText(true);
    const int pausedIndex = viz.addDecoration(
            SimTK::MobilizedBodyIndex(0), SimTK::Vec3(0), pausedText);

    int istate = 0;

    bool paused = false;

    while (true) {
        if (istate == numStates) {
            istate = 0;
            // Without this line, all but the first replay will be shown as
            // fast as possible rather than as real-time.
            viz.setMode(SimTK::Visualizer::RealTime);
        }

        // Slider input.
        int sliderIndex;
        double sliderValue;
        if (silo.takeSliderMove(sliderIndex, sliderValue)) {
            if (sliderIndex == realTimeScaleSliderIndex) {
                viz.setRealTimeScale(sliderValue);
            } else if (sliderIndex == timeSliderIndex) {
                // index = [seconds] * [# states / second]
                istate = (int)SimTK::clamp(0,
                        (sliderValue - initialTime) * dataRate, numStates - 1);
                // Allow the user to drag this slider to visualize different
                // times.
                viz.drawFrameNow(statesTraj[istate]);
            } else {
                std::cout << "Internal error: unrecognized slider."
                          << std::endl;
            }
        }

        // Key input.
        unsigned key, modifiers;
        if (silo.takeKeyHit(key, modifiers)) {
            // Exit.
            if (key == SimTK::Visualizer::InputListener::KeyEsc) {
                std::cout << "Exiting visualization." << std::endl;
                return;
            }
            // Smart zoom.
            else if (key == 'r') {
                viz.zoomCameraToShowAllGeometry();
            }
            // Pause.
            else if (key == ' ') {
                paused = !paused;
                auto& text = static_cast<SimTK::DecorativeText&>(
                        viz.updDecoration(pausedIndex));
                text.setText(paused ? "Paused (hit Space to resume)" : "");
                // Show the updated text.
                viz.drawFrameNow(statesTraj[istate]);
            }
        }

        viz.setSliderValue(realTimeScaleSliderIndex, viz.getRealTimeScale());
        viz.setSliderValue(timeSliderIndex,
                std::round((istate / dataRate + initialTime) * 1000) / 1000);

        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            viz.report(statesTraj[istate]);
            ++istate;
        }
    }
}

void OpenSim::visualize(Model model, TimeSeriesTable table) {
    visualize(std::move(model), convertTableToStorage(table));
}

void OpenSim::prescribeControlsToModel(
        const MocoIterate& iterate, Model& model) {
    // Get actuator names.
    model.initSystem();
    OpenSim::Array<std::string> actuNames;
    const auto modelPath = model.getAbsolutePath();
    for (const auto& actu : model.getComponentList<Actuator>()) {
        actuNames.append(actu.getAbsolutePathString());
    }

    // Add prescribed controllers to actuators in the model, where the control
    // functions are splined versions of the actuator controls from the OCP
    // solution.
    const SimTK::Vector& time = iterate.getTime();
    auto* controller = new PrescribedController();
    controller->setName("prescribed_controller");
    for (int i = 0; i < actuNames.size(); ++i) {
        const auto control = iterate.getControl(actuNames[i]);
        auto* function = new GCVSpline(5, time.nrow(), &time[0], &control[0]);
        const auto& actu = model.getComponent<Actuator>(actuNames[i]);
        controller->addActuator(actu);
        controller->prescribeControlForActuator(actu.getName(), function);
    }
    model.addController(controller);
}

MocoIterate OpenSim::simulateIterateWithTimeStepping(
        const MocoIterate& iterate, Model model, double integratorAccuracy) {

    prescribeControlsToModel(iterate, model);

    // Add states reporter to the model.
    auto* statesRep = new StatesTrajectoryReporter();
    statesRep->setName("states_reporter");
    statesRep->set_report_time_interval(0.001);
    model.addComponent(statesRep);

    // Simulate!
    const SimTK::Vector& time = iterate.getTime();
    SimTK::State state = model.initSystem();
    state.setTime(time[0]);
    Manager manager(model);

    // Set the initial state.
    {
        const auto& matrix = iterate.getStatesTrajectory();
        TimeSeriesTable initialStateTable(
                std::vector<double>{iterate.getInitialTime()},
                SimTK::Matrix(matrix.block(0, 0, 1, matrix.ncol())),
                iterate.getStateNames());
        auto statesTraj = StatesTrajectory::createFromStatesStorage(
                model, convertTableToStorage(initialStateTable));
        state.setY(statesTraj.front().getY());
    }

    if (integratorAccuracy != -1) {
        manager.getIntegrator().setAccuracy(integratorAccuracy);
    }
    manager.initialize(state);
    state = manager.integrate(time[time.size() - 1]);

    // Export results from states reporter to a TimeSeries Table
    TimeSeriesTable states = statesRep->getStates().exportToTable(model);

    const auto& statesTimes = states.getIndependentColumn();
    SimTK::Vector timeVec((int)statesTimes.size(), statesTimes.data(), true);
    TimeSeriesTable controls = resample(model.getControlsTable(), timeVec);
    // Fix column labels. (TODO: Not general.)
    auto labels = controls.getColumnLabels();
    for (auto& label : labels) { label = "/forceset/" + label; }
    controls.setColumnLabels(labels);

    // Create a MocoIterate to facilitate states trajectory comparison (with
    // dummy data for the multipliers, which we'll ignore).

    auto forwardSolution = MocoIterate(timeVec,
            {{"states", {states.getColumnLabels(), states.getMatrix()}},
                    {"controls", {controls.getColumnLabels(),
                                         controls.getMatrix()}}});

    return forwardSolution;
}

void OpenSim::replaceMusclesWithPathActuators(Model& model) {

    // Create path actuators from muscle properties and add to the model. Save
    // a list of pointers of the muscles to delete.
    std::vector<Muscle*> musclesToDelete;
    auto& muscleSet = model.updMuscles();
    for (int i = 0; i < muscleSet.getSize(); ++i) {
        auto& musc = muscleSet.get(i);
        auto* actu = new PathActuator();
        actu->setName(musc.getName());
        musc.setName(musc.getName() + "_delete");
        actu->setOptimalForce(musc.getMaxIsometricForce());
        actu->setMinControl(musc.getMinControl());
        actu->setMaxControl(musc.getMaxControl());

        const auto& pathPointSet = musc.getGeometryPath().getPathPointSet();
        auto& geomPath = actu->updGeometryPath();
        for (int i = 0; i < pathPointSet.getSize(); ++i) {
            auto* pathPoint = pathPointSet.get(i).clone();
            const auto& socketNames = pathPoint->getSocketNames();
            for (const auto& socketName : socketNames) {
                pathPoint->updSocket(socketName)
                        .connect(pathPointSet.get(i)
                                         .getSocket(socketName)
                                         .getConnecteeAsObject());
            }
            geomPath.updPathPointSet().adoptAndAppend(pathPoint);
        }
        model.addComponent(actu);
        musclesToDelete.push_back(&musc);
    }

    // Delete the muscles.
    for (const auto* musc : musclesToDelete) {
        int index = model.getForceSet().getIndex(musc, 0);
        OPENSIM_THROW_IF(index == -1, Exception,
                format("Muscle with name %s not found in ForceSet.",
                        musc->getName()));
        bool success = model.updForceSet().remove(index);
        OPENSIM_THROW_IF(!success, Exception,
                format("Attempt to remove muscle with "
                       "name %s was unsuccessful.",
                        musc->getName()));
    }
}

void OpenSim::removeMuscles(Model& model) {

    // Save a list of pointers of the muscles to delete.
    std::vector<Muscle*> musclesToDelete;
    auto& muscleSet = model.updMuscles();
    for (int i = 0; i < muscleSet.getSize(); ++i) {
        musclesToDelete.push_back(&muscleSet.get(i));
    }

    // Delete the muscles.
    for (const auto* musc : musclesToDelete) {
        int index = model.getForceSet().getIndex(musc, 0);
        OPENSIM_THROW_IF(index == -1, Exception,
                format("Muscle with name %s not found in ForceSet.",
                        musc->getName()));
        model.updForceSet().remove(index);
    }

    model.finalizeConnections();
    model.initSystem();
}

void OpenSim::createReserveActuators(Model& model, double optimalForce) {
    OPENSIM_THROW_IF(optimalForce <= 0, Exception,
            format("Invalid value (%g) for create_reserve_actuators; "
                   "should be -1 or positive.",
                    optimalForce));

    std::cout << "Adding reserve actuators with an optimal force of "
              << optimalForce << "..." << std::endl;

    Model modelCopy(model);
    auto state = modelCopy.initSystem();
    std::vector<std::string> coordPaths;
    // Borrowed from
    // CoordinateActuator::CreateForceSetOfCoordinateAct...
    for (const auto& coord : modelCopy.getComponentList<Coordinate>()) {
        if (!coord.isConstrained(state)) {
            auto* actu = new CoordinateActuator();
            actu->setCoordinate(
                    &model.updComponent<Coordinate>(
                            coord.getAbsolutePathString()));
            auto path = coord.getAbsolutePathString();
            coordPaths.push_back(path);
            // Get rid of model name.
            // Get rid of slashes in the path; slashes not allowed in names.
            std::replace(path.begin(), path.end(), '/', '_');
            actu->setName("reserve_" + path);
            actu->setOptimalForce(optimalForce);
            model.addForce(actu);
        }
    }
    // Re-make the system, since there are new actuators.
    model.initSystem();
    std::cout << "Added " << coordPaths.size()
              << " reserve actuator(s), "
                 "for each of the following coordinates:"
              << std::endl;
    for (const auto& name : coordPaths) {
        std::cout << "  " << name << std::endl;
    }
}

void OpenSim::replaceJointWithWeldJoint(
        Model& model, const std::string& jointName) {
    OPENSIM_THROW_IF(!model.getJointSet().hasComponent(jointName), Exception,
            "Joint with name '" + jointName +
                    "' not found in the model JointSet.");

    // This is needed here to access offset frames.
    model.finalizeConnections();

    // Get the current joint and save a copy of the parent and child offset
    // frames.
    auto& current_joint = model.updJointSet().get(jointName);
    PhysicalOffsetFrame* parent_offset = PhysicalOffsetFrame().safeDownCast(
            current_joint.getParentFrame().clone());
    PhysicalOffsetFrame* child_offset = PhysicalOffsetFrame().safeDownCast(
            current_joint.getChildFrame().clone());

    // Save the original names of the body frames (not the offset frames), so we
    // can find them when the new joint is created.
    parent_offset->finalizeConnections(model);
    child_offset->finalizeConnections(model);
    std::string parent_body_path =
            parent_offset->getParentFrame().getAbsolutePathString();
    std::string child_body_path =
            child_offset->getParentFrame().getAbsolutePathString();

    // Remove the current Joint from the the JointSet.
    model.updJointSet().remove(&current_joint);

    // Create the new joint and add it to the model.
    auto* new_joint = new WeldJoint(jointName,
            model.getComponent<PhysicalFrame>(parent_body_path),
            parent_offset->get_translation(), parent_offset->get_orientation(),
            model.getComponent<PhysicalFrame>(child_body_path),
            child_offset->get_translation(), child_offset->get_orientation());
    model.addJoint(new_joint);

    model.finalizeConnections();
}

void OpenSim::addCoordinateActuatorsToModel(Model& model, double optimalForce) {
    OPENSIM_THROW_IF(optimalForce <= 0, Exception,
        "The optimal force must be greater than zero.");

    std::cout << "Adding reserve actuators with an optimal force of "
              << optimalForce << "..." << std::endl;

    std::vector<std::string> coordPaths;
    const auto& state = model.getWorkingState();
    // Borrowed from 
    // CoordinateActuator::CreateForceSetOfCoordinateActuatorsForModel().
    // TODO: just use model.updComponentList<Coordinate>() and avoid const_cast?
    for (const auto& constCoord : model.getComponentList<Coordinate>()) {
        auto* actu = new CoordinateActuator();
        auto& coord = const_cast<Coordinate&>(constCoord);
        if (coord.isConstrained(state)) continue;

        actu->setCoordinate(&coord);
        auto path = coord.getAbsolutePathString();
        coordPaths.push_back(path);
        // Get rid of model name.
        // Get rid of slashes in the path; slashes not allowed in names.
        std::replace(path.begin(), path.end(), '/', '_');
        if (path.at(0) == '_') { path.erase(0, 1); }
        actu->setName(path);
        actu->setOptimalForce(optimalForce);
        model.addComponent(actu);
        model.initSystem();
    }

    // Re-make the system, since there are new actuators.
    model.initSystem();
    std::cout << "Added " << coordPaths.size()
              << " reserve actuator(s), for each of the following coordinates:"
              << std::endl;
    for (const auto& name : coordPaths) {
        std::cout << "  " << name << std::endl;
    }

}

std::vector<std::string> OpenSim::createStateVariableNamesInSystemOrder(
        const Model& model) {
    std::unordered_map<int, int> yIndexMap;
    return createStateVariableNamesInSystemOrder(model, yIndexMap);
}
std::vector<std::string> OpenSim::createStateVariableNamesInSystemOrder(
        const Model& model, std::unordered_map<int, int>& yIndexMap) {
    yIndexMap.clear();
    std::vector<std::string> svNamesInSysOrder;
    auto s = model.getWorkingState();
    const auto svNames = model.getStateVariableNames();
    s.updY() = 0;
    std::vector<int> yIndices;
    for (int iy = 0; iy < s.getNY(); ++iy) {
        s.updY()[iy] = SimTK::NaN;
        const auto svValues = model.getStateVariableValues(s);
        for (int isv = 0; isv < svNames.size(); ++isv) {
            if (SimTK::isNaN(svValues[isv])) {
                svNamesInSysOrder.push_back(svNames[isv]);
                yIndices.emplace_back(iy);
                s.updY()[iy] = 0;
                break;
            }
        }
        if (SimTK::isNaN(s.updY()[iy])) {
            // If we reach here, this is an unused slot for a quaternion.
            s.updY()[iy] = 0;
        }
    }
    int count = 0;
    for (const auto& iy : yIndices) {
        yIndexMap.emplace(std::make_pair(count, iy));
        ++count;
    }
    SimTK_ASSERT2_ALWAYS((size_t)svNames.size() == svNamesInSysOrder.size(),
            "Expected to get %i state names but found %i.", svNames.size(),
            svNamesInSysOrder.size());
    return svNamesInSysOrder;
}

std::unordered_map<std::string, int> OpenSim::createSystemYIndexMap(
        const Model& model) {
    std::unordered_map<std::string, int> sysYIndices;
    auto s = model.getWorkingState();
    const auto svNames = model.getStateVariableNames();
    s.updY() = 0;
    for (int iy = 0; iy < s.getNY(); ++iy) {
        s.updY()[iy] = SimTK::NaN;
        const auto svValues = model.getStateVariableValues(s);
        for (int isv = 0; isv < svNames.size(); ++isv) {
            if (SimTK::isNaN(svValues[isv])) {
                sysYIndices[svNames[isv]] = iy;
                s.updY()[iy] = 0;
                break;
            }
        }
        if (SimTK::isNaN(s.updY()[iy])) {
            // If we reach here, this is an unused slot for a quaternion.
            s.updY()[iy] = 0;
        }
    }
    SimTK_ASSERT2_ALWAYS(svNames.size() == (int)sysYIndices.size(),
            "Expected to find %i state indices but found %i.", svNames.size(),
            sysYIndices.size());
    return sysYIndices;
}

std::string OpenSim::format_c(const char* format, ...) {
    // Get buffer size.
    va_list args;
    va_start(args, format);
    int bufsize = vsnprintf(nullptr, 0, format, args) + 1; // +1 for '\0'
    va_end(args);

    // Create formatted string.
    std::unique_ptr<char[]> buf(new char[bufsize]);
    va_start(args, format);
    vsnprintf(buf.get(), bufsize, format, args);
    va_end(args);
    return std::string(buf.get());
}

int OpenSim::getMocoParallelEnvironmentVariable() {
    const std::string varName = "OPENSIM_MOCO_PARALLEL";
    if (SimTK::Pathname::environmentVariableExists(varName)) {
        std::string parallel = SimTK::Pathname::getEnvironmentVariable(varName);
        int num = std::atoi(parallel.c_str());
        if (num < 0) {
            std::cout << "[Moco] Warning: OPENSIM_MOCO_PARALLEL "
                         "environment variable set to incorrect value '"
                      << parallel
                      << "'; must be an integer >= 0. "
                         "Ignoring."
                      << std::endl;
        } else {
            return num;
        }
    }
    return -1;
}
