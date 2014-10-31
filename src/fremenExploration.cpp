#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <sensor_msgs/JointState.h>

#include "fremen/Entropy.h"
#include "fremen/AddView.h"
#include "fremen/Visualize.h"

#define MIN_X  -5.8
#define MIN_Y  -19.0
#define MIN_Z  0.0
#define DIM_X 250
#define DIM_Y 500
#define DIM_Z 80
#define RESOLUTION 0.05

#define MAX_ENTROPY 132000

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

using namespace std;
bool ptuMovementFinished = true;

//Parameters
double exploration_radius, entropy_step;
int nr_points;

ros::Publisher ptu_pub;
sensor_msgs::JointState ptu;

void movePtu(float pan,float tilt)
{
	ptuMovementFinished = false;
	ptu.name[0] ="pan";
	ptu.name[1] ="tilt";
	ptu.position[0] = pan;
	ptu.position[1] = tilt;
	ptu.velocity[0] = ptu.velocity[1] = 1.0;
	ptu_pub.publish(ptu);
}

void ptuCallback(const sensor_msgs::JointState::ConstPtr &msg)
{
	for (int i = 0;i<3;i++){
		if (msg->name[i] == "pan"){
			//printf("Pan %i %.3f - %.3f = %.3f\n",ptuMovementFinished,msg->position[i],ptu.position[0],msg->position[i]-ptu.position[0]);
			if (fabs(msg->position[i]-ptu.position[0])<0.01) ptuMovementFinished = true;
		}
	}
}

int main(int argc,char *argv[])
{
    ros::init(argc, argv, "FremenExploration");
    ros::NodeHandle n;

    ros::NodeHandle nh("~");
    nh.param("exploration_radius", exploration_radius, 2.0);
    nh.param("nr_points", nr_points, 12);
    nh.param("interval", entropy_step, 1.0);

    //tell the action client that we want to spin a thread by default
    MoveBaseClient ac("move_base", true);

    ROS_INFO("Starting exploration node...");

    tf::TransformListener tf_listener;

    //Publisher (Visualization of Points + Entropy Values)
    ros::Publisher points_pub = n.advertise<visualization_msgs::MarkerArray>("/entropy_points", 100);
    ros::Publisher text_pub = n.advertise<visualization_msgs::MarkerArray>("/entropy_values", 100);

    ptu.name.resize(2);
    ptu.position.resize(2);
    ptu.velocity.resize(2);
    ptu_pub = n.advertise<sensor_msgs::JointState>("/ptu/cmd", 10);

    geometry_msgs::Point position, next_position, marker_point;

    ros::Subscriber ptu_sub = n.subscribe("/ptu/state", 10, ptuCallback);

    //Entropy Client
    ros::ServiceClient entropy_client = n.serviceClient<fremen::Entropy>("/fremenGrid/entropy");
    fremen::Entropy entropy_srv;

    //Vizualize Client
    ros::ServiceClient visualize_client = n.serviceClient<fremen::Visualize>("/fremenGrid/visualize");
    fremen::Visualize visualize_srv;

    //Measure Client
    ros::ServiceClient measure_client = n.serviceClient<fremen::AddView>("/fremenGrid/measure");
    fremen::AddView measure_srv;

    //Move_Base Client
    move_base_msgs::MoveBaseGoal goal;
    goal.target_pose.header.frame_id = "/map";

    //get robot pose
    tf::StampedTransform st;

    //Markers Initialization
    visualization_msgs::MarkerArray points_markers, values_markers;

    visualization_msgs::Marker test_point;
    test_point.header.frame_id = "/map";
    test_point.header.stamp = ros::Time::now();
    test_point.ns = "my_namespace";
    test_point.action = visualization_msgs::Marker::ADD;
    test_point.type = visualization_msgs::Marker::SPHERE;
    test_point.scale.x = 0.3;
    test_point.scale.y = 0.3;
    test_point.scale.z = 0.3;
    test_point.color.a = 0.6;
    test_point.color.r = 0.1;
    test_point.color.g = 0.0;
    test_point.color.b = 1.0;
    test_point.pose.position.z = 0.1;
    test_point.pose.orientation.w = 1.0;

    visualization_msgs::Marker text_point;
    text_point.header.frame_id = "/map";
    text_point.header.stamp = ros::Time::now();
    text_point.ns = "my_namespace";
    text_point.action = visualization_msgs::Marker::ADD;
    text_point.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    text_point.scale.z = 0.1;
    text_point.color.a = 1.0;
    text_point.color.r = 1.0;
    text_point.color.g = 1.0;
    text_point.color.b = 1.0;
    text_point.pose.position.z = 0.1;
    text_point.pose.orientation.w = 1.0;

    //Entropy Grid
    unsigned int nr_x, nr_y;
    nr_x = ((DIM_X*RESOLUTION-entropy_step)/entropy_step);
    nr_y = ((DIM_Y*RESOLUTION-entropy_step)/entropy_step);

    //PTU:
    float ptuSweepStep = 2.0*M_PI/7.0;
    float ptuAngle = -3*ptuSweepStep;
    sleep(1);
    movePtu(ptuAngle,0);
    usleep(10000);
    while (ros::ok()){
	    while (ros::ok() && ptuAngle < M_PI)
	    {
		    measure_srv.request.stamp = 0.0;
		    if (ptuMovementFinished){
			    if(measure_client.call(measure_srv))
			    {
				    ROS_INFO("Measure added to grid!");
			    }
			    else
			    {
				    ROS_ERROR("Failed to call measure service");
				    return 1;
			    }
			    ptuAngle += ptuSweepStep; 
			    usleep(100000);
			    movePtu(ptuAngle,0);
			    usleep(100000);

			    visualize_srv.request.red = visualize_srv.request.blue = 0.0;
			    visualize_srv.request.green = visualize_srv.request.alpha = 1.0;
			    visualize_srv.request.minProbability = 0.9;
			    visualize_srv.request.maxProbability = 1.0;
			    visualize_srv.request.name = "occupied";
			    visualize_srv.request.type = 0;
			    visualize_client.call(visualize_srv);
			    ros::spinOnce();
			    usleep(100000);

			    visualize_srv.request.green = 0.0;
			    visualize_srv.request.red = 1.0;
			    visualize_srv.request.minProbability = 0.0;
			    visualize_srv.request.maxProbability = 0.1;
			    visualize_srv.request.alpha = 0.005;
			    visualize_srv.request.name = "free";
			    visualize_srv.request.type = 0;
			    visualize_client.call(visualize_srv);
			    ros::spinOnce();
			    usleep(100000);



		    }
		    ros::spinOnce();
	    }
	    ptuAngle = 0;
	    movePtu(ptuAngle,0);

	    try {
		    tf_listener.waitForTransform("/map","/base_link",ros::Time::now(), ros::Duration(2));
		    tf_listener.lookupTransform("/map","/base_link",ros::Time(0),st);

		    position.x = st.getOrigin().x();
		    position.y = st.getOrigin().y();

		    float new_entropy = 0.0, old_entropy = 0.0;

		    for(float i = 0; i < 2*M_PI; i+= 2*M_PI/nr_points)
		    {
			    entropy_srv.request.x = position.x + exploration_radius * cos(i);
			    entropy_srv.request.y = position.y + exploration_radius * sin(i);
			    entropy_srv.request.z = 1.69;
			    entropy_srv.request.r = 4;
			    entropy_srv.request.t = 0.0;


			    //Entropy Srv
			    if(entropy_client.call(entropy_srv)>0)
			    {
				    ROS_INFO("Entropy at Point (%f,%f) is %.3f", entropy_srv.request.x, entropy_srv.request.y, entropy_srv.response.value);
				    new_entropy = entropy_srv.response.value;
			    }
			    else
			    {
				    ROS_ERROR("Failed to call entropy service");
				    return 1;
			    }


			    if(new_entropy > old_entropy)
			    {
				    old_entropy = new_entropy;
				    //                    ROS_ERROR("here!");
				    next_position.x = entropy_srv.request.x;
				    next_position.y = entropy_srv.request.y;
				    next_position.z = entropy_srv.request.z;

			    }

		    }

		    //Move Base
		    ROS_INFO("Moving to point (%f,%f)...", next_position.x, next_position.y);
		    goal.target_pose.header.stamp = ros::Time::now();
		    goal.target_pose.pose.position.x = next_position.x;
		    goal.target_pose.pose.position.y = next_position.y;
		    goal.target_pose.pose.orientation.w = 1.0;

		    ac.sendGoal(goal);
		    ac.waitForResult();

		    if(ac.getState() == actionlib::SimpleClientGoalState::SUCCEEDED)
			    ROS_INFO("Hooray!");
		    else
			    ROS_INFO("The base failed to move for some reason");

	    }
	    catch (tf::TransformException ex) {
		    ROS_ERROR("FreMeEn map cound not incorporate the latest measurements %s",ex.what());
		    return 0;
	    }
	   ros::spin();
    }

    return 0;
}
