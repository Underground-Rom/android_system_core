/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NETWORKMANAGER_H
#define _NETWORKMANAGER_H

#include <sysutils/SocketListener.h>

#include "Controller.h"

#include "PropertyManager.h"

class InterfaceConfig;

class NetworkManager {
private:
    static NetworkManager *sInstance;

private:
    ControllerCollection *mControllers;
    SocketListener       *mBroadcaster;
    PropertyManager      *mPropMngr;

public:
    virtual ~NetworkManager();

    int run();

    int attachController(Controller *controller);

    Controller *findController(const char *name);

    void setBroadcaster(SocketListener *sl) { mBroadcaster = sl; }
    SocketListener *getBroadcaster() { return mBroadcaster; }
    PropertyManager *getPropMngr() { return mPropMngr; }

    static NetworkManager *Instance();

private:
    int startControllers();
    int stopControllers();

    NetworkManager(PropertyManager *propMngr);

public:
    /*
     * Called from a controller when an interface is available/ready for use.
     * 'cfg' contains information on how this interface should be configured.
     */
    int onInterfaceStart(Controller *c, const InterfaceConfig *cfg);

    /*
     * Called from a controller when an interface should be shut down
     */
    int onInterfaceStop(Controller *c, const char *name);
};
#endif
