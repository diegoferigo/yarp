/*
 * Copyright (C) 2006-2020 Istituto Italiano di Tecnologia (IIT)
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the
 * BSD-3-Clause license. See the accompanying LICENSE file for details.
 */

#include "Robot.h"

#include <yarp/os/LogStream.h>

#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/PolyDriverList.h>

#include "Action.h"
#include "Device.h"
#include "Param.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>


std::ostringstream& operator<<(std::ostringstream& oss, const yarp::robotinterface::Robot& t)
{
    oss << "(name = \"" << t.name() << "\"";
    if (!t.params().empty()) {
        oss << ", params = [";
        oss << t.params();
        oss << "]";
    }
    if (!t.devices().empty()) {
        oss << ", devices = [";
        oss << t.devices();
        oss << "]";
    }
    oss << ")";
    return oss;
}


class yarp::robotinterface::Robot::Private
{
public:
    Private(Robot* /*parent*/) :
            build(0),
            currentPhase(ActionPhaseUnknown),
            currentLevel(0)
    {
    }

    // return true if a device with the given name exists
    bool hasDevice(const std::string& name) const;

    // return the device with the given name or <fatal error> if not found
    Device* findDevice(const std::string& name);

    // open all the devices and return true if all the open calls were successful
    bool openDevices();

    // close all the devices and return true if all the close calls were successful
    bool closeDevices();

    // return a vector of levels that have actions in the requested phase
    std::vector<unsigned int> getLevels(ActionPhase phase) const;

    // return a vector of actions for that phase and that level
    std::vector<std::pair<Device, Action>> getActions(ActionPhase phase, unsigned int level) const;


    // run configure action on one device
    bool configure(const Device& device, const ParamList& params);

    // run calibrate action on one device
    bool calibrate(const Device& device, const ParamList& params);

    // run attach action on one device
    bool attach(const Device& device, const ParamList& params);

    // run abort action on one device
    bool abort(const Device& device, const ParamList& params);

    // run detach action on one device
    bool detach(const Device& device, const ParamList& params);

    // run park action on one device
    bool park(const Device& device, const ParamList& params);

    // run custom action on one device
    bool custom(const Device& device, const ParamList& params);

    // optionally merge xml devices with external ones
    yarp::robotinterface::DeviceList allDevicesWithExternal() const;

    std::string name;
    unsigned int build;
    std::string portprefix;
    ParamList params;
    DeviceList devices;
    DeviceList externalDevices;
    yarp::robotinterface::ActionPhase currentPhase;
    unsigned int currentLevel;
}; // class yarp::robotinterface::Robot::Private

bool yarp::robotinterface::Robot::Private::hasDevice(const std::string& name) const
{
    for (const auto& device : allDevicesWithExternal()) {
        if (name == device.name()) {
            return true;
        }
    }

    return false;
}

yarp::robotinterface::DeviceList yarp::robotinterface::Robot::Private::allDevicesWithExternal() const
{
    using DeviceName = std::string;
    std::unordered_map<DeviceName, Device> extDevicesMap;

    for (const auto& externalDevice : externalDevices) {
        extDevicesMap[externalDevice.name()] = externalDevice;
    }

    yarp::robotinterface::DeviceList allDevices;

    for (auto& device : devices) {

        if (extDevicesMap.find(device.name()) != extDevicesMap.end()) {
            allDevices.push_back(extDevicesMap.at(device.name()));
        } else {
            allDevices.push_back(device);
        }
    }

    return allDevices;
}

yarp::robotinterface::Device* yarp::robotinterface::Robot::Private::findDevice(const std::string& name)
{
    for (auto& device : allDevicesWithExternal()) {
        if (name == device.name()) {
            return &device;
        }
    }

    yFatal() << "Cannot find device" << name;
    return nullptr;
}

bool yarp::robotinterface::Robot::Private::openDevices()
{
    bool ret = true;
    for (auto& device : allDevicesWithExternal()) {

        if (device.isOpen()) {
            yDebug() << "Device" << device.name() << "is already open";
            continue;
        }

        // Open the device if it has not been already opened
        if (!device.open()) {
            yWarning() << "Cannot open device" << device.name();
            ret = false;
        }
    }
    if (ret) {
        // yDebug() << "All devices opened.";
    } else {
        yWarning() << "There was some problem opening one or more devices. Please check the log and your configuration";
    }

    return ret;
}

bool yarp::robotinterface::Robot::Private::closeDevices()
{
    bool ret = true;
    for (auto it = devices.rbegin(); it != devices.rend(); ++it) {
        yarp::robotinterface::Device& device = *it;

        // yDebug() << device;

        if (!device.close()) {
            yWarning() << "Cannot close device" << device.name();
            ret = false;
        }
    }
    if (ret) {
        // yDebug() << "All devices closed.";
    } else {
        yWarning() << "There was some problem closing one or more devices. Please check the log and your configuration";
    }

    return ret;
}

std::vector<unsigned int> yarp::robotinterface::Robot::Private::getLevels(yarp::robotinterface::ActionPhase phase) const
{
    std::vector<unsigned int> levels;
    for (const auto& device : allDevicesWithExternal()) {

        if (device.actions().empty()) {
            continue;
        }

        for (const auto& action : device.actions()) {
            if (action.phase() == phase) {
                levels.push_back(action.level());
            }
        }
    }

    std::sort(levels.begin(), levels.end());
    auto it = std::unique(levels.begin(), levels.end());
    levels.resize(it - levels.begin());

    return levels;
}


std::vector<std::pair<yarp::robotinterface::Device, yarp::robotinterface::Action>> yarp::robotinterface::Robot::Private::getActions(yarp::robotinterface::ActionPhase phase, unsigned int level) const
{
    std::vector<std::pair<yarp::robotinterface::Device, yarp::robotinterface::Action>> actions;

    for (const auto& device : allDevicesWithExternal()) {
        if (device.actions().empty()) {
            continue;
        }

        for (const auto& action : device.actions()) {
            if (action.phase() == phase && action.level() == level) {
                actions.emplace_back(device, action);
            }
        }
    }
    return actions;
}


bool yarp::robotinterface::Robot::Private::configure(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    YARP_FIXME_NOTIMPLEMENTED("configure action")
    return true;
}

bool yarp::robotinterface::Robot::Private::calibrate(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    if (!yarp::robotinterface::hasParam(params, "target")) {
        yError() << "Action \"" << ActionTypeToString(ActionTypeCalibrate) << R"(" requires "target" parameter)";
        return false;
    }
    std::string targetDeviceName = yarp::robotinterface::findParam(params, "target");

    if (!hasDevice(targetDeviceName)) {
        yError() << "Target device" << targetDeviceName << "does not exist.";
        return false;
    }
    Device& targetDevice = *findDevice(targetDeviceName);

    return device.calibrate(targetDevice);
}

bool yarp::robotinterface::Robot::Private::attach(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    int check = 0;
    if (yarp::robotinterface::hasParam(params, "network"))
        check++;
    if (yarp::robotinterface::hasParam(params, "networks"))
        check++;
    if (yarp::robotinterface::hasParam(params, "all"))
        check++;

    if (check > 1) {
        yError() << "Action \"" << ActionTypeToString(ActionTypeAttach) << R"(" : you can have only one option: "network" , "networks" or "all" )";
        return false;
    }

    yarp::dev::PolyDriverList drivers;

    if (yarp::robotinterface::hasParam(params, "network")) {
        std::string targetNetwork = yarp::robotinterface::findParam(params, "network");

        if (!yarp::robotinterface::hasParam(params, "device")) {
            yError() << "Action \"" << ActionTypeToString(ActionTypeAttach) << R"(" requires "device" parameter)";
            return false;
        }
        std::string targetDeviceName = yarp::robotinterface::findParam(params, "device");

        if (!hasDevice(targetDeviceName)) {
            yError() << "Target device" << targetDeviceName << "(network =" << targetNetwork << ") does not exist.";
            return false;
        }
        Device& targetDevice = *findDevice(targetDeviceName);

        // yDebug() << "Attach device" << device.name() << "to" << targetDevice.name() << "as" << targetNetwork;
        drivers.push(targetDevice.driver(), targetNetwork.c_str());

    } else if (yarp::robotinterface::hasParam(params, "all")) {
        for (auto& device : allDevicesWithExternal()) {
            drivers.push(device.driver(), "all");
        }
    } else if (yarp::robotinterface::hasParam(params, "networks")) {
        yarp::os::Value v;
        v.fromString(yarp::robotinterface::findParam(params, "networks").c_str());
        yarp::os::Bottle& targetNetworks = *(v.asList());

        for (size_t i = 0; i < targetNetworks.size(); ++i) {
            std::string targetNetwork = targetNetworks.get(i).toString();

            if (!yarp::robotinterface::hasParam(params, targetNetwork)) {
                yError() << "Action \"" << ActionTypeToString(ActionTypeAttach) << "\" requires one parameter per network. \"" << targetNetwork << "\" parameter is missing.";
                return false;
            }
            std::string targetDeviceName = yarp::robotinterface::findParam(params, targetNetwork);
            if (!hasDevice(targetDeviceName)) {
                yError() << "Target device" << targetDeviceName << "(network =" << targetNetwork << ") does not exist.";
                return false;
            }
            Device& targetDevice = *findDevice(targetDeviceName);

            // yDebug() << "Attach device" << device.name() << "to" << targetDevice.name() << "as" << targetNetwork;
            drivers.push(targetDevice.driver(), targetNetwork.c_str());
        }
    } else {
        yError() << "Action \"" << ActionTypeToString(ActionTypeAttach) << R"(" requires either "network" or "networks" parameter)";
        return false;
    }

    if (!drivers.size()) {
        yError() << "Action \"" << ActionTypeToString(ActionTypeAttach) << "\" couldn't find any device.";
        return false;
    }

    return device.attach(drivers);
}

bool yarp::robotinterface::Robot::Private::abort(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    YARP_FIXME_NOTIMPLEMENTED("abort action")
    return true;
}


bool yarp::robotinterface::Robot::Private::detach(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{

    if (!params.empty()) {
        yWarning() << "Action \"" << ActionTypeToString(ActionTypeDetach) << "\" cannot have any parameter. Ignoring them.";
    }

    return device.detach();
}

bool yarp::robotinterface::Robot::Private::park(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    if (!yarp::robotinterface::hasParam(params, "target")) {
        yError() << "Action \"" << ActionTypeToString(ActionTypePark) << R"(" requires "target" parameter)";
        return false;
    }
    std::string targetDeviceName = yarp::robotinterface::findParam(params, "target");

    if (!hasDevice(targetDeviceName)) {
        yError() << "Target device" << targetDeviceName << "does not exist.";
        return false;
    }
    Device& targetDevice = *findDevice(targetDeviceName);

    return device.park(targetDevice);
}

bool yarp::robotinterface::Robot::Private::custom(const yarp::robotinterface::Device& device, const yarp::robotinterface::ParamList& params)
{
    YARP_FIXME_NOTIMPLEMENTED("custom action")
    return true;
}

yarp::os::LogStream operator<<(yarp::os::LogStream dbg, const yarp::robotinterface::Robot& t)
{
    std::ostringstream oss;
    oss << t;
    dbg << oss.str();
    return dbg;
}

yarp::robotinterface::Robot::Robot() :
        mPriv(new Private(this))
{
}

yarp::robotinterface::Robot::Robot(const std::string& name, const yarp::robotinterface::DeviceList& devices) :
        mPriv(new Private(this))
{
    mPriv->name = name;
    mPriv->devices = devices;
}

yarp::robotinterface::Robot::Robot(const yarp::robotinterface::Robot& other) :
        mPriv(new Private(this))
{
    mPriv->name = other.mPriv->name;
    mPriv->build = other.mPriv->build;
    mPriv->portprefix = other.mPriv->portprefix;
    mPriv->currentPhase = other.mPriv->currentPhase;
    mPriv->currentLevel = other.mPriv->currentLevel;
    mPriv->devices = other.mPriv->devices;
    mPriv->params = other.mPriv->params;
}

yarp::robotinterface::Robot& yarp::robotinterface::Robot::operator=(const yarp::robotinterface::Robot& other)
{
    if (&other != this) {
        mPriv->name = other.mPriv->name;
        mPriv->build = other.mPriv->build;
        mPriv->portprefix = other.mPriv->portprefix;
        mPriv->currentPhase = other.mPriv->currentPhase;
        mPriv->currentLevel = other.mPriv->currentLevel;

        mPriv->devices.clear();
        mPriv->devices = other.mPriv->devices;

        mPriv->params.clear();
        mPriv->params = other.mPriv->params;
    }

    return *this;
}

yarp::robotinterface::Robot::~Robot()
{
    delete mPriv;
}

std::string& yarp::robotinterface::Robot::name()
{
    return mPriv->name;
}

unsigned int& yarp::robotinterface::Robot::build()
{
    return mPriv->build;
}

std::string& yarp::robotinterface::Robot::portprefix()
{
    return mPriv->portprefix;
}

void yarp::robotinterface::Robot::setVerbose(bool verbose)
{
    for (auto& device : devices()) {
        ParamList& params = device.params();
        // Do not override "verbose" param if explicitly set in the xml
        if (verbose && !yarp::robotinterface::hasParam(params, "verbose")) {
            device.params().push_back(Param("verbose", "1"));
        }
    }
}

void yarp::robotinterface::Robot::setAllowDeprecatedDevices(bool allowDeprecatedDevices)
{
    for (auto& device : devices()) {
        ParamList& params = device.params();
        // Do not override "allow-deprecated-devices" param if explicitly set in the xml
        if (allowDeprecatedDevices && !yarp::robotinterface::hasParam(params, "allow-deprecated-devices")) {
            device.params().push_back(Param("allow-deprecated-devices", "1"));
        }
    }
}

yarp::robotinterface::ParamList& yarp::robotinterface::Robot::params()
{
    return mPriv->params;
}

yarp::robotinterface::DeviceList& yarp::robotinterface::Robot::devices()
{
    return mPriv->devices;
}

yarp::robotinterface::Device& yarp::robotinterface::Robot::device(const std::string& name)
{
    return *mPriv->findDevice(name);
}

const std::string& yarp::robotinterface::Robot::name() const
{
    return mPriv->name;
}

const unsigned int& yarp::robotinterface::Robot::build() const
{
    return mPriv->build;
}

const std::string& yarp::robotinterface::Robot::portprefix() const
{
    return mPriv->portprefix;
}

const yarp::robotinterface::ParamList& yarp::robotinterface::Robot::params() const
{
    return mPriv->params;
}

const yarp::robotinterface::DeviceList& yarp::robotinterface::Robot::devices() const
{
    return mPriv->devices;
}

const yarp::robotinterface::Device& yarp::robotinterface::Robot::device(const std::string& name) const
{
    return *mPriv->findDevice(name);
}

void yarp::robotinterface::Robot::interrupt()
{
    yInfo() << "Interrupt received. Stopping all running threads.";

    // If we received an interrupt we send a stop signal to all threads
    // from previous phases
    for (auto& device : devices()) {
        device.stopThreads();
    }
}

bool yarp::robotinterface::Robot::enterPhase(
    yarp::robotinterface::ActionPhase phase,
    const DeviceList& externalDevices)
{
    yInfo() << ActionPhaseToString(phase) << "phase starting...";

    // Store the external devices
    mPriv->externalDevices = externalDevices;

    // Remove external devices with RAII
    auto deleter = [&](void*) {
        mPriv->externalDevices = {};
    };
    std::unique_ptr<void, decltype(deleter)> raiiExtDevices{nullptr, deleter};

    mPriv->currentPhase = phase;
    mPriv->currentLevel = 0;

    // Open all devices
    if (phase == ActionPhaseStartup) {
        if (!mPriv->openDevices()) {
            yError() << "One or more devices failed opening... see previous log messages for more info";
            if (!mPriv->closeDevices()) {
                yError() << "One or more devices failed closing";
            }
            return false;
        }
    }

    // run phase does not accept actions
    if (phase == ActionPhaseRun) {
        if (mPriv->getLevels(phase).size() != 0) {
            yWarning() << "Phase" << ActionPhaseToString(phase) << "does not accept actions. Skipping all actions for this phase";
        }
        return true;
    }

    // Before starting any action we ensure that there are no other
    // threads running from prevoius phases.
    // In interrupt 2 and 3 and this is called by the interrupt callback,
    // and therefore main thread will be blocked and join will never
    // return. Therefore, since we want to start the abort actions we
    // skip this check.
    if (phase != ActionPhaseInterrupt2 && phase != ActionPhaseInterrupt3) {
        for (auto& device : devices()) {
            device.joinThreads();
        }
    }

    std::vector<unsigned int> levels = mPriv->getLevels(phase);

    bool ret = true;
    for (std::vector<unsigned int>::const_iterator lit = levels.begin(); lit != levels.end(); ++lit) {
        // for each level
        const unsigned int level = *lit;

        yInfo() << "Entering action level" << level << "of phase" << ActionPhaseToString(phase);
        mPriv->currentLevel = level;

        // If current phase was changed by some other thread, we should
        // exit the loop and avoid starting further actions.
        if (mPriv->currentPhase != phase) {
            ret = false;
            break;
        }

        std::vector<std::pair<Device, Action>> actions = mPriv->getActions(phase, level);

        for (auto& ait : actions) {
            // for each action in that level
            Device& device = ait.first;
            Action& action = ait.second;

            // If current phase was changed by some other thread, we should
            // exit the loop and avoid starting further actions.
            if (mPriv->currentPhase != phase) {
                ret = false;
                break;
            }

            switch (action.type()) {
            case ActionTypeConfigure:
                if (!mPriv->configure(device, action.params())) {
                    yError() << "Cannot run configure action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypeCalibrate:
                if (!mPriv->calibrate(device, action.params())) {
                    yError() << "Cannot run calibrate action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypeAttach:
                if (!mPriv->attach(device, action.params())) {
                    yError() << "Cannot run attach action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypeAbort:
                if (!mPriv->abort(device, action.params())) {
                    yError() << "Cannot run abort action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypeDetach:
                if (!mPriv->detach(device, action.params())) {
                    yError() << "Cannot run detach action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypePark:
                if (!mPriv->park(device, action.params())) {
                    yError() << "Cannot run park action on device" << device.name();
                    ret = false;
                }
                break;
            case ActionTypeCustom:
                if (!mPriv->custom(device, action.params())) {
                    yError() << "Cannot run custom action on device" << device.name();
                    ret = false;
                }
                break;
            default:
                yWarning() << "Unhandled action" << ActionTypeToString(action.type());
                ret = false;
                break;
            }
        }

        yInfo() << "All actions for action level" << level << "of" << ActionPhaseToString(phase) << "phase started. Waiting for unfinished actions.";

        // Join parallel threads
        for (auto& device : devices()) {
            device.joinThreads();
            // yDebug() << "All actions for device" << device.name() << "at level()" << level << "finished";
        }

        yInfo() << "All actions for action level" << level << "of" << ActionPhaseToString(phase) << "phase finished.";
    }

    if (!ret) {
        yWarning() << "There was some problem running actions for" << ActionPhaseToString(phase) << "phase . Please check the log and your configuration";
    }

    if (phase == ActionPhaseShutdown) {
        if (!mPriv->closeDevices()) {
            yError() << "One or more devices failed closing";
            return false;
        }
    }

    yInfo() << ActionPhaseToString(phase) << "phase finished.";

    return ret;
}

yarp::robotinterface::ActionPhase yarp::robotinterface::Robot::currentPhase() const
{
    return mPriv->currentPhase;
}

int yarp::robotinterface::Robot::currentLevel() const
{
    return mPriv->currentLevel;
}

bool yarp::robotinterface::Robot::hasParam(const std::string& name) const
{
    return yarp::robotinterface::hasParam(mPriv->params, name);
}

std::string yarp::robotinterface::Robot::findParam(const std::string& name) const
{
    return yarp::robotinterface::findParam(mPriv->params, name);
}
