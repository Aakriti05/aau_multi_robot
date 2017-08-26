#ifndef DOCKING_STATION_MANAGER_INTERFACE_H
#define DOCKING_STATION_MANAGER_INTERFACE_H

#include "ds_structs.h"

class DockingStationManagerInterface
{
public:
    virtual void getDockingStations() = 0;
    virtual void addDockingStation(ds_t ds) = 0;
};

#endif // DOCKING_STATION_MANAGER_INTERFACE_H