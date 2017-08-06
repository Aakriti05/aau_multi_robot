#ifndef ROBOT_STATE_MANAGER_H
#define ROBOT_STATE_MANAGER_H

#include <ros/ros.h>
#include <ros/package.h>
#include <ros/topic.h>
#include <boost/thread/mutex.hpp>
#include "robot_state/robot_state.h"
#include "robot_state/GetRobotState.h"
#include "robot_state/SetRobotState.h"
#include "robot_state/LockRobotState.h"
#include "robot_state/UnlockRobotState.h"

//TODO ???
#define USAGE "\nUSAGE: map_server <map.yaml>\n" \
              "  map.yaml: map description file\n" \
              "DEPRECATED USAGE: map_server <map> <resolution>\n" \
              "  map: image file to load\n"\
              "  resolution: map resolution [meters/pixel]"

#define LOG_TRUE    true
#define LOG_FALSE   false
#define LOG_ALL     true

class RobotStateManager
{
public:
    RobotStateManager();

private:
    unsigned int robot_state;
    std::vector <std::string> robot_state_strings;
    boost::mutex mutex;
    
    ros::ServiceServer ss_get_robot_state;
    ros::ServiceServer ss_set_robot_state;
    ros::ServiceServer ss_lock_robot_state;
    ros::ServiceServer ss_unlock_robot_state;

    void createServices();
    bool get_robot_state_callback(robot_state::GetRobotState::Request &req, robot_state::GetRobotState::Response &res);
    bool set_robot_state_callback(robot_state::SetRobotState::Request &req, robot_state::SetRobotState::Response &res);
    bool lock_robot_state_callback(robot_state::LockRobotState::Request &req, robot_state::LockRobotState::Response &res);
    bool unlock_robot_state_callback(robot_state::UnlockRobotState::Request &req, robot_state::UnlockRobotState::Response &res);

    std::string robotStateEnumToString(unsigned int enum_value);
    void fillRobotStateStringsVector();
};

#endif /* ROBOT_STATE_MANAGER_H */