#ifndef BATTERY_SIMULATE_H
#define BATTERY_SIMULATE_H

/*
class battery_simulate : public battery {
public:
    battery_simulate() : battery();
};
*/

#include <ros/ros.h>
#include <std_msgs/Empty.h>
#include <geometry_msgs/Twist.h>
#include <diagnostic_msgs/DiagnosticArray.h>
#include <energy_mgmt/battery_state.h>
#include <explorer/Speed.h>
#include "std_msgs/Float32.h"
#include <adhoc_communication/EmRobot.h>

class battery_simulate
{
public:
    /**
     * Constructor.
     */
    battery_simulate();

    /**
     * Compute the battery state.
     */
    void compute();

    /**
     * Outputs the battery soc to the console.
     */
    void output();

    /**
     * Publishes a message containing the battery state.
     */
    void publish();

    /**
     * The battery state.
     * This includes:
     *  - bool charging
     *  - float32 soc
     *  - float32 remaining_time_charge
     *  - float32 remaining_time_run
     *  - float32 remaining_distance
     */
    energy_mgmt::battery_state state;
    
    void run();


private:
    /**
     * The node handle.
     */
    ros::NodeHandle nh;

    /**
     * Subscribers for the required topics.
     */
    ros::Subscriber sub_charge, sub_cmd_vel, sub_speed, sub_soc, sub_time;

    /**
     * Publishers.
     */
    ros::Publisher pub_battery, pub_full_battery_info;

    /**
     * Battery charge in Wh.
     * Charge is the current battery charge, charge_max is the charge if the battery is fully charged.
     */
    double charge, charge_max;

    /**
     * The power consumption of the robot.
     * Power_charging is the power that the robot is charged with.
     * Power_moving is the power consumption of the robot while in motion.
     * Power_standing is the power consumption of the robot while it is standing still.
     */
    double power_charging, power_moving, power_standing;

    /**
     * Speed of the robot.
     * The values are received from subscribed topics.
     */
    double speed_linear, speed_angular, speed_avg, speed_avg_init;

    /**
     * Total time the robot has been standing and moving.
     * This excludes the times while the robot is recharging.
     */
    double time_moving, time_standing;

    /**
     * Percentage of time the robot has been standing and moving.
     */
    double perc_moving, perc_standing;

    /**
     * Whether or not the soc output has been shown.
     * This is necessary so that each value is shown only once.
     */
    bool output_shown;

    /**
     * TODO
     * NEW, do we need this???
     */
    double total_time;

    /**
     * The last time that the computations where carried out.
     */
    ros::Time time_last;

    /**
     * Callback that is triggered when the robot should start recharging.
     */
    void cb_charge(const std_msgs::Empty::ConstPtr &msg);

    /**
     * Callback that is used to determine if robot is standing or moving.
     * In the cmd_vel message there are only two important parameters, the x-linear value and the z-angular value.
     * When they are zero the robots stands still. If the x-value is unequal to zero then the robot moves forward or
     * backward and if the z-value is unequal to zero the robot rotates.
     * When the robot stands still it consumes less energy and when it moves it consumes more energy.
     */
    void cb_cmd_vel(const geometry_msgs::Twist &msg);

    /**
     * Callback that is used to get the average speed of the robot.
     */
    void cb_speed(const explorer::Speed &msg);

    /**
     * Callback that is used to get the robot's state of charge.
     */
    void cb_soc(const std_msgs::Float32::ConstPtr& msg);

    /**
     * TODO: do we need this?
     * Callback to get maximum time.
     */
    void totalTime(const std_msgs::Float32::ConstPtr& msg);
    
    // Maximum speed of the robot
    double max_speed_linear;
    
    double remaining_energy, total_energy, maximum_running_time;
    
    ros::Publisher pub_charging_completed;
    ros::Subscriber sub_robot;
    
    
    enum state_t
    {
        exploring,       // the robot is computing which is the next frontier to be
                         // explored
        going_charging,  // the robot has the right to occupy a DS to recharge
        charging,        // the robot is charging at a DS
        finished,        // the robot has finished the exploration
        fully_charged,   // the robot has recently finished a charging process; notice
                         // that the robot is in this state even if it is not really
                         // fully charged (since just after a couple of seconds after
                         // the end of the recharging process the robot has already
                         // lost some battery energy, since it consumes power even
                         // when it stays still
        stuck,
        in_queue,                                  // the robot is in a queue, waiting for a DS to be vacant
        auctioning,                                // auctioning: the robot has started an auction; notice that if
                                                   // the robot is aprticipating to an auction that it was not
                                                   // started by it, its state is not equal to auctioning!!!
        auctioning_2,
        going_in_queue,                            // the robot is moving near a DS to later put itself in
                                                   // in_queue state
        going_checking_vacancy,                    // the robot is moving near a DS to check if it
                                                   // vacant, so that it can occupy it and start
                                                   // recharging
        checking_vacancy,                          // the robot is currently checking if the DS is vacant,
                                                   // i.e., it is waiting information from the other robots
                                                   // about the state of the DS
        moving_to_frontier_before_going_charging,  // TODO hmm...
        moving_to_frontier,                         // the robot has selected the next frontier to be
                                                   // reached, and it is moving toward it
        leaving_ds                                 //the robot was recharging, but another robot stopped                                           
    };
    
    void cb_robot(const adhoc_communication::EmRobot::ConstPtr &msg);
    
    float mass;
    
    
};


#endif //BATTERY_SIMULATE_H