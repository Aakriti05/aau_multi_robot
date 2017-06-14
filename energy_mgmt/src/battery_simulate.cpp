#include <battery_simulate.h>

#define INFINITE_ENERGY 0

using namespace std;

//TODO(minor) comments, debugs, and so on...

battery_simulate::battery_simulate() 
{
    // read parameters
    nh.getParam("energy_mgmt/speed_avg", speed_avg_init);
    nh.getParam("energy_mgmt/power_charging", power_charging); // W (i.e, watt)
    nh.getParam("energy_mgmt/power_moving", power_moving);     // W/(m/s)
    nh.getParam("energy_mgmt/power_standing", power_standing); // W
    nh.getParam("energy_mgmt/charge_max", charge_max);         // Wh (i.e, watt-hour) //TODO(minor) which value is good in the YAML file?
    nh.getParam("energy_mgmt/max_linear_speed", max_speed_linear); // m/s
    nh.getParam("energy_mgmt/mass", mass); // kg
    power_basic_computations = 8.0; // W
    power_advanced_computation = 3.0;
    advanced_computations_bool = true;
    
    //ROS_ERROR("%f, %f, %f, %f", power_charging, power_moving, power_standing, charge_max);

    // initialize private variables
    speed_linear = 0;
    speed_angular = 0;
    time_moving = 0;   //TODO(minor) useless?
    time_standing = 0; //TODO(minor) useless?
    perc_moving = 0.5; //TODO(minor) useless?
    perc_standing = 0.5; //TODO(minor) useless?
    output_shown = false; //TODO(minor) useless?
    speed_avg = speed_avg_init;
    
    charge = charge_max;
    total_energy = charge_max * 3600; // J (i.e, joule)       
    remaining_energy = total_energy;
    speed_linear = max_speed_linear;
    maximum_running_time = total_energy / (power_moving * max_speed_linear + power_standing + power_basic_computations + power_advanced_computation); // s //TODO power_advanced_computation is missing a final 's'
    
    //ROS_ERROR("maximum_running_time: %f", maximum_running_time);
    
    if(INFINITE_ENERGY) {
        total_energy = 10000;
        remaining_energy = 10000;
        power_standing = 0.0;  
        power_moving   = 0.0;  
        power_charging = 100.0;
        maximum_running_time = 100000000000000000;
    }

    // initialize battery state
    state.charging = false;
    state.soc = 1; // (adimensional) // TODO(minor) if we assume that the robot starts fully_charged
    state.remaining_time_charge = 0; //TODO(minor) -1? (but everywhere if yes...)
    state.remaining_time_run = maximum_running_time; //s //TODO(minor) "maximum" is misleading: use "estimated"...
    state.remaining_distance = maximum_running_time * speed_avg_init; //m //TODO(minor) explain why we use max_speed_linear and not min_speed, etc.

    // advertise topics
    pub_battery = nh.advertise<energy_mgmt::battery_state>("battery_state", 1);
    pub_charging_completed = nh.advertise<std_msgs::Empty>("charging_completed", 1);
    
    pub_full_battery_info = nh.advertise<energy_mgmt::battery_state>("full_battery_info", 1);

    // subscribe to topics
    sub_speed = nh.subscribe("avg_speed", 1, &battery_simulate::cb_speed, this);
    sub_cmd_vel = nh.subscribe("cmd_vel", 1, &battery_simulate::cb_cmd_vel, this);
    sub_robot = nh.subscribe("explorer/robot", 100, &battery_simulate::cb_robot, this);    
    sub_time = nh.subscribe("totalTime", 1, &battery_simulate::totalTime, this); // TODO(minor) do we need this?

    //ROS_ERROR("remaining distance: %f", state.remaining_distance);
    pub_full_battery_info.publish(state);
    
    time_last = ros::Time::now();
}

void battery_simulate::cb_robot(const adhoc_communication::EmRobot::ConstPtr &msg)
{
    if (msg.get()->state == charging)
    {
        ROS_DEBUG("Start recharging");
        state.charging = true;
    }
    else {
        if(msg.get()->state == fully_charged || msg.get()->state == leaving_ds || msg.get()->state == exploring)
            advanced_computations_bool = true;
        else
            advanced_computations_bool = false;
    
        /* The robot is not charging; if the battery was previously under charging, it means that the robot aborted the
           recharging process */
        if (state.charging == true)
        {
            ROS_DEBUG("Recharging aborted");
            state.charging = false;
        }
        
           
    }
}

void battery_simulate::compute()
{
    /* Compute the number of elapsed seconds since the last time that we updated the battery state (since we use
     * powers) */
    ros::Duration time_diff = ros::Time::now() - time_last;
    double time_diff_sec = time_diff.toSec();
    
    /* If there is no time difference to last computation, there is nothing to do */
    if (time_diff_sec <= 0) {
        return;
    }

    /* If the robot is charging, increase remaining battery life, otherwise compute consumed energy and decrease remaining battery life */
    if (state.charging)
    {
        ROS_DEBUG("Recharging...");
        remaining_energy += power_charging * time_diff_sec;
        state.soc = remaining_energy / total_energy;
        state.remaining_time_charge = -1; //TODO

        /* Check if the battery is now fully charged; notice that SOC could be higher than 100% due to how we increment
         * the remaing_energy during the charging process */
        if (state.soc >= 1)
        {
            ROS_INFO("Recharging completed");
            state.soc = 1; // since SOC cannot be higher than 100% in real life, force it to be 100%
            state.charging = false;
            state.remaining_time_charge = 0;
            remaining_energy = total_energy;
            state.remaining_time_run = maximum_running_time;
            state.remaining_distance = maximum_running_time * max_speed_linear;
            std_msgs::Empty msg; //TODO(minor) use remaining_time_charge instead of the publisher
            pub_charging_completed.publish(msg);
        }
        else
        {
            state.remaining_time_charge = (total_energy - remaining_energy) / power_charging;
            state.remaining_time_run = state.soc * total_energy;
            state.remaining_distance = state.remaining_time_run * speed_avg;
        }
    }
    else
    {
        /* If the robot is moving, than we have to consider also the energy consumed for moving, otherwise it is
         * sufficient to consider the fixed power cost.
         * Notice that this is clearly an approximation, since we are not sure that the robot was moving for the whole
         * interval of time: moreover, since we do not know the exact speed profile during this interval of time, we
         * overestimate the consumed energy by assuming that the robot moved at the maximum speed for the whole period.
         */
        int mult; //TODO check better
        if(advanced_computations_bool)
            mult = 1;
        else
            mult = 0;
        if (speed_linear > 0 || speed_angular > 0)
            remaining_energy -= (power_moving * max_speed_linear + power_standing + power_basic_computations + power_advanced_computation * mult) * time_diff_sec; // J
        else
            remaining_energy -= (                                  power_standing + power_basic_computations + power_advanced_computation * mult) * time_diff_sec; // J

        //ROS_ERROR("%f", remaining_energy);
        /* Update battery state */
        state.soc = remaining_energy / total_energy;
        
        //state.remaining_time_run = state.soc * total_energy; // NO!!!
        //state.remaining_time_run = mass * speed_avg / total_energy; // in s, since J = kg*m/s^2
        state.remaining_time_run = remaining_energy / (power_moving * max_speed_linear + power_standing + power_basic_computations + power_advanced_computation);
        
        state.remaining_distance = state.remaining_time_run * speed_avg; 

    }
    
    //ROS_ERROR("Battery: %.0f%%  ---  remaining distance: %.2fm", state.soc * 100, state.remaining_distance);
    
    /* Store the time at which this battery state update has been perfomed, so that next time we can compute againg the elapsed time interval */
    time_last = ros::Time::now();
}

void battery_simulate::output()
{
    /*
    if ((int(state.soc) % 10) == 0)
    {
        if (output_shown == false)
        {
            ROS_INFO("Battery: %.0f%%  ---  remaining distance: %.2fm", state.soc, state.remaining_distance);
            output_shown = true;
        }
    }
    else
    */
        output_shown = false;
}

void battery_simulate::publish()
{
    pub_battery.publish(state);
    //ROS_ERROR("%.1f", state.soc);
}

void battery_simulate::cb_cmd_vel(const geometry_msgs::Twist &msg)
{
    ROS_DEBUG("Received speed");
    speed_linear = msg.linear.x;
    speed_angular = msg.angular.z;
}

void battery_simulate::cb_speed(const explorer::Speed &msg)  // unused for the moment, but maybe with a better estimate
                                                             // of the battery curve...
{
    // if the average speed is very low, there is probably something wrong, set it to the value from the config file
    if (msg.avg_speed > speed_avg_init)
    {
        speed_avg = msg.avg_speed;
    }
    else
    {
        speed_avg = speed_avg_init;
    }
}

// TODO(minor) - DO WE NEED THIS???
void battery_simulate::cb_soc(const std_msgs::Float32::ConstPtr &msg)
{
    // ROS_INFO("Received SOC!!!");
}

// TODO(minor) do we need this?
void battery_simulate::totalTime(const std_msgs::Float32::ConstPtr &msg)
{
    total_time = ("%F", msg->data);
}

void battery_simulate::run() {
    while(ros::ok()) {
        ros::Duration(5).sleep(); //TODO(minor) rates???
        ros::spinOnce();
        
        // compute new battery state
        compute();

        // output battery state to console
        //bat.output();

        // publish battery state
        publish();
    }
}
