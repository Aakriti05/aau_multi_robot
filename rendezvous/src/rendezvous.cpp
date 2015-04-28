#include "rendezvous.h"

Rendezvous::Rendezvous()
{
    nh = new ros::NodeHandle("~");

    // parameter
    nh->param<std::string>("robot_prefix", robot_prefix, "");
    nh->param<int>("waitForResult", waitForResult, 3);
    nh->param<std::string>("move_base_frame",move_base_frame,"map");

    this->rendezvousPoints = new std::vector<RendezvousPoint>;

    // create clients for explorer services
    std::string service = robot_prefix + std::string("/explorer/switchExplRend");
    expl_client = nh->serviceClient<explorer::switchExplRend>(service.c_str());

    service = robot_prefix + std::string("/explorer/getPosition");
    position_client = nh->serviceClient<explorer::getPosition>(service.c_str());

    service = robot_prefix + std::string("/explorer/MissionFinished");
    mission_client = nh->serviceClient<explorer::MissionFinished>(service.c_str());

    // wait till information about home is available
    ros::Duration(20).sleep();

    // get and set home point
    explorer::getPosition srv;
    srv.request.name = "home";
    if((this->position_client.call(srv)) == false)
    {
        ROS_ERROR("Failed to call explorer service 'getPosition'");
    }
    else
    {
        this->home_x = srv.response.x;
        this->home_y = srv.response.y;
    }

    // add home point as first element to vector of rendezvous points
    addRendezvous(home_x, home_y);
}

void Rendezvous::exploreRobot()
{
    // explore till timeout
    explorer::switchExplRend srv_msg;
    srv_msg.request.explore = true;
    if(expl_client.call(srv_msg) == false)
    {
        ROS_ERROR("Failed to call explorer service 'switchExplRend'!");
    }

    ROS_DEBUG("EXPLORER will now explore for 40 sec!");

    ros::Duration(40).sleep();

    callbackMoveToRendezvous();
}



void Rendezvous::callbackMoveToRendezvous()
{
    ROS_DEBUG("EXPLORER  Timeout happened, robot should move back to rendezvous!");

    // stop explorer
    explorer::switchExplRend srv_msg;
    srv_msg.request.explore = false;
    if(expl_client.call(srv_msg) == false)
    {
        ROS_ERROR("Failed to call explorer service 'switchExplRend'!");
    }

    // get current robot position
    double rend_x;
    double rend_y;
    explorer::getPosition srv;
    srv.request.name = "currentPos";
    if(this->position_client.call(srv) == false)
    {
        ROS_ERROR("Failed to call explorer service 'getPosition'");
    }
    else
    {
        rend_x = srv.response.x;
        rend_y = srv.response.y;
    }

    addRendezvous(rend_x, rend_y);

    // publish current point as next rendezvous
    rendezvous::RendezvousPoint msg;
    msg.x = rend_x;
    msg.y = rend_y;
    pub.publish(msg);
    ROS_DEBUG("EXPLORER published new Rendezvous Point: x = %f, y = %f", msg.x, msg.y);

    // determine last not visited rendezvous, move there and wait for 25 sec at rendezvous
    for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
        i != rendezvousPoints->end(); ++i)
    {
        if(i->visited == false){
            currentRendezvous = *i;
            ROS_DEBUG("EXPLORER should move to x = %f , y = %f (= its current rendezvous)!", currentRendezvous.x, currentRendezvous.y);
            break;
        }
    }

    if(currentRendezvous.x == home_x && currentRendezvous.y == home_y)
    {
        ROS_DEBUG("EXPLORER: Current rendezvous is home, explorer will not move there!");
    }
    else
    {
        if(move_robot(currentRendezvous.x, currentRendezvous.y)){
            ROS_DEBUG("EXPLORER at Rendezvous now. It will wait here for 5 sec!");
            ros::Duration(5).sleep();
        }
        else
        {
            ROS_DEBUG("EXPLORER failed to move to rendezvous!");
        }
     }

    // mark rendezvous as visited
    // case 1: robot reached rendezvous -> marking ok
    // case 2: robot failed to reach rendezvous -> probably rendezvous not reachable, robot should not try again .. TODO
    // case 3: rendezvous == home -> marking ok, bc explore robot should not return home till mission is finished
    for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
        i != rendezvousPoints->end(); ++i)
    {
        if(i->x == currentRendezvous.x && i->y == currentRendezvous.y){
            i->visited = true;
        }
    }

//    ROS_DEBUG("EXPLORER: Vector of rendezvous points after adding current position as new one");
//    printRendezvousPoints();

//    // move to last not visited rendezvous .. should be home first time
//    for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
//        i != rendezvousPoints->end(); ++i)
//    {
//        if(i->visited == false){
//            currentRendezvous = *i;
//            break;
//        }
//    }
//    ROS_DEBUG("EXPLORER should move to x = %f , y = %f (= its current rendezvous)!", currentRendezvous.x, currentRendezvous.y);

//    if(move_robot(currentRendezvous.x, currentRendezvous.y) == true)
//    {
//        // this rendezvous was visited
//        for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
//            i != rendezvousPoints->end(); ++i)
//        {
//            if(i->x == currentRendezvous.x && i->y == currentRendezvous.y){
//                i->visited = true;
//            }
//        }
//        ROS_INFO("EXPLORER at rendezvous now!");
//        ros::Duration(5).sleep();

//        // TODO: wait for RELAY & exchange information
//    }
//    else
//    {
//        ROS_ERROR("Failed to move robot to rendezvous!");
//    }

//    ROS_DEBUG("EXPLORER: Vector of rendezvous points after visiting current rendezvous");
//    printRendezvousPoints();

    // (if mission is not finished) explore again till next timeout

    explorer::MissionFinished mission_srv;
    if(this->mission_client.call(mission_srv) == false)
    {
        ROS_ERROR("Failed to call explorer service 'missionFinished'");
    }
    else if(mission_srv.response.finished == false)          // if mission is not finished, explore further
    {
        ROS_DEBUG("EXPLORER will explore further because mission is not finished.");
        exploreRobot();
    }
    else                                            // if mission is finished, start explorer
    {
        ROS_DEBUG("EXPLORER :  mission finished !!!!");
        ROS_DEBUG("will start explorer again, should return home");
        srv_msg.request.explore = true;
        if(expl_client.call(srv_msg) == false)
        {
            ROS_ERROR("Failed to call explorer service 'switchExplRend'!");
        }
    }


}

// function of RELAY; will be called when its explorer publishes their next rendezvous
void Rendezvous::new_Rendezvous_available(const rendezvous::RendezvousPointConstPtr& msg)
{
    ROS_DEBUG("RELAY received Point x = %f, y = %f", msg->x, msg->y);

    // first change point a little to avoid crush then add it to own vector of rendezvous
    addRendezvous((msg->x)+0.5, (msg->y)+0.5);

    // TODO: check if manipulated point is still in map, else change it in another direction

    ROS_DEBUG("RELAY: Vector of rendezvous points");
    printRendezvousPoints();
}


void Rendezvous::relayRobot()
{
    // stop explorer
    explorer::switchExplRend srv_msg;
    srv_msg.request.explore = false;

    if(this->expl_client.call(srv_msg) == false)
    {
        ROS_DEBUG("In Rendezvous::relayRobot()");
        ROS_ERROR("Failed to call explorer service 'switchExplRend'");
    } else {
        ROS_DEBUG("RELAY stopped explorer!");
    }

    explorer::MissionFinished mission_srv;
    if(this->mission_client.call(mission_srv) == false)
    {
        ROS_ERROR("Failed to call explorer service 'missionFinished'");
    } else {
        missionFinished = mission_srv.response.finished;
    }

    while(missionFinished == false)
    {

        // if new rendezvous available, get it
        ros::spinOnce();

        // move to last not visited rendezvous or stay if there isn't any new rendezvous
        for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
            i != rendezvousPoints->end(); ++i)
        {
            if(i->visited == false)
            {
                currentRendezvous = *i;
                ROS_DEBUG("RELAY should move to x = %f , y = %f (= its current rendezvous)!", currentRendezvous.x, currentRendezvous.y);
                break;
            }
            else
            {
                // no new rendezvous available -> stay
            }
        }

        if(move_robot(currentRendezvous.x, currentRendezvous.y) == true)
        {
            // mark rendezvous as visited
            for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
                i != rendezvousPoints->end(); ++i)
            {
                if(i->x == currentRendezvous.x && i->y == currentRendezvous.y){
                    i->visited = true;
                }
            }

            ROS_DEBUG("RELAY at rendezvous, it will return to Home in 10 sec.");
            ros::Duration(10).sleep();

            // TODO: wait there for EXPLORE robot

            if(move_robot(home_x, home_y))
            {
                 ROS_DEBUG("RELAY at home, it will wait there for 20 sec.");

                 if(this->mission_client.call(mission_srv) == false)
                 {
                     ROS_ERROR("Failed to call explorer service 'missionFinished'");
                 } else {
                     missionFinished = mission_srv.response.finished;
                 }


            }

            ros::Duration(20).sleep();

        }
        else
        {
            ROS_ERROR("Failed to move robot to rendezvous!");
            //ROS_DEBUG("Try to move it to x = %f, y = %f", currentRendezvous.x-0.4, currentRendezvous.y-0.4);
            //move_robot((currentRendezvous.x-0.4), (currentRendezvous.y-0.4));

            // mark rendezvous as visited
            for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
                i != rendezvousPoints->end(); ++i)
            {
                if(i->x == currentRendezvous.x && i->y == currentRendezvous.y){
                    i->visited = true;
                }
            }

            ros::Duration(20).sleep();
        }

        if(this->mission_client.call(mission_srv) == false)
        {
            ROS_ERROR("Failed to call explorer service 'missionFinished'");
        } else {
            missionFinished = mission_srv.response.finished;
        }
    }

    ROS_DEBUG("RELAY :  mission finished !!!!");
}
    // get current robot position (:= rendezvous)
//    double rend_x;
//    double rend_y;
//    explorer::getPosition srv;
//    srv.request.name = "";
//    if(this->position_client.call(srv) == false)
//    {
//        ROS_DEBUG("In Rendezvous::relayRobot()");
//        ROS_ERROR("Failed to call explorer service 'getPosition'");
//    }
//    else
//    {
//        rend_x = srv.response.x;
//        rend_y = srv.response.y;
//        ROS_DEBUG("RELAY stopped at point x = %f \t y = %f", rend_x, rend_y);
//    }

    // continue moving between home and rendezvous
//    while(true)
//    {
//        if(this->move_robot(home_x, home_y) == false)
//        {
//            ROS_ERROR("Failed to move robot to home position");
//        }
//        else
//        {
//            ROS_DEBUG("RELAY at home now!");
//            ros::Duration(5).sleep();
//        }

//        if(this->move_robot(rend_x, rend_y) == false)
//        {
//            ROS_ERROR("Failed to move robot to rendezvous");
//        }
//        else
//        {
//            ROS_DEBUG("RELAY at Rendezvous now!");
//            ros::Duration(5).sleep();
//        }

//    }


void Rendezvous::addRendezvous(double new_x, double new_y)
{
    RendezvousPoint rend;
    rend.x = new_x;
    rend.y = new_y;
    rend.visited = false;
    rendezvousPoints->push_back(rend);
}

void Rendezvous::printRendezvousPoints()
{
    int n = 1;
    for(std::vector<RendezvousPoint>::iterator i = rendezvousPoints->begin();
        i != rendezvousPoints->end(); ++i)
    {
        ROS_DEBUG("%d - th Point: x = %f \t y = %f \t visited = %d \n", n, i->x, i->y, i->visited);
        n++;
    }

}

// method from explorer
bool Rendezvous::move_robot(double position_x, double position_y)
{
    /*
     * Move the robot with the help of an action client. Goal positions are
     * transmitted to the robot and feedback is given about the actual
     * driving state of the robot.
     */

    MoveBaseClient ac("move_base", true);

    while (!ac.waitForServer(ros::Duration(10.0)));

    move_base_msgs::MoveBaseGoal goal_msgs;

    goal_msgs.target_pose.header.frame_id = move_base_frame;
    goal_msgs.target_pose.pose.position.x = position_x;
    goal_msgs.target_pose.pose.position.y = position_y;
    goal_msgs.target_pose.pose.position.z = 0;
    goal_msgs.target_pose.pose.orientation.x = 0;
    goal_msgs.target_pose.pose.orientation.y = 0;
    goal_msgs.target_pose.pose.orientation.z = 0;
    goal_msgs.target_pose.pose.orientation.w = 1;

    ac.sendGoal(goal_msgs);

    ac.waitForResult(ros::Duration(waitForResult));
    while (ac.getState() == actionlib::SimpleClientGoalState::PENDING)
    {
        ros::Duration(0.5).sleep();
    }

    while (ac.getState() == actionlib::SimpleClientGoalState::ACTIVE)
    {
        ros::Duration(0.5).sleep();
    }

    while (ac.getState() != actionlib::SimpleClientGoalState::SUCCEEDED)
    {
        if (ac.getState() == actionlib::SimpleClientGoalState::ABORTED)
        {
            return false;
        }
    }
    return true;
}

// Test function
void Rendezvous::stopStartExplorer()
{
    // robots should stop explorer after 40 sec,
    // wait for 30 sec and start explorer again

    ros::Duration(40).sleep();

    ROS_DEBUG("%s  should stop explorer now!", robot_prefix.c_str());
    explorer::switchExplRend srv_msg;
    srv_msg.request.explore = false;

    if(expl_client.call(srv_msg) == false)
    {
        ROS_ERROR("Failed to call explorer service 'switchExplRend'!");
    }

    ros::Duration(30).sleep();

    ROS_DEBUG("%s  should start explorer again!", robot_prefix.c_str());
    srv_msg.request.explore = true;
    if(expl_client.call(srv_msg) == false)
    {
        ROS_ERROR("Failed to call explorer service 'switchExplRend'!");
    }

}