import threading
import rclpy
from rclpy.node import Node

from gazebo_msgs.msg import LinkStates
from sensor_msgs.msg import Imu
from generalized_pose_msgs.msg import DesiredGeneralizedPose

from .planners.planner_mjp import MotionPlanner

from .utils.quat_math import q_mult
from .utils.fading_filter import FadingFilter

import numpy as np



class MinimalSubscriber(Node):
    def __init__(self):
        super().__init__("minimal_subscriber")
        self.link_states_subscription = self.create_subscription(
            LinkStates,
            "/gazebo/link_states",
            self.link_states_callback,
            1)
        
        self.link_states_subscription # prevent unused variable warning

        self.imu_subscription = self.create_subscription(
            Imu,
            "/imu_sensor_broadcaster/imu",
            self.imu_callback,
            1
        )

        self.imu_subscription # prevent unused variable warning

        self.q_b = np.array([])
        self.v_b = np.array([])
        self.a_b = np.array([])


    # Save the base pose and twist
    def link_states_callback(self, msg):
        # The index [1] is used because the first link ([0]) is the ground_plane. The second one (the index [1]) corresponds to anymal base.
        base_id = 1

        # /gazebo/link_states returns the pose and the twist in the inertial or world frame.
        
        # Extract the base position and orientation (quaternion)
        pos = msg.pose[base_id].position
        orient = msg.pose[base_id].orientation

        # Extract the base linear and angular velocity
        lin = msg.twist[base_id].linear
        ang = msg.twist[base_id].angular

        # Save the base pose and twists as numpy arrays
        self.q_b = np.array([pos.x, pos.y, pos.z, orient.x, orient.y, orient.z, orient.w])
        self.v_b = np.array([lin.x, lin.y, lin.z, ang.x, ang.y, ang.z])


    # Save the base acceleration
    def imu_callback(self, msg):
        acc = msg.linear_acceleration

        self.a_b = np.array([acc.x, acc.y, acc.z])



class MinimalPublisher(Node):
    def __init__(self):
        super().__init__("minimal_publisher")
        self.publisher_ = self.create_publisher(DesiredGeneralizedPose, 'robot/desired_generalized_pose', 1)

    def publish_desired_gen_pose(
        self,
        contactFeet,
        r_b_ddot_des, r_b_dot_des, r_b_des,
        omega_des, q_des,
        r_s_ddot_des, r_s_dot_des, r_s_des
    ):
        # Initialize and fully populate the desired generalized pose message

        msg = DesiredGeneralizedPose()

        msg.base_acc.x = r_b_ddot_des[0]
        msg.base_acc.y = r_b_ddot_des[1]
        msg.base_acc.z = r_b_ddot_des[2]

        msg.base_vel.x = r_b_dot_des[0]
        msg.base_vel.y = r_b_dot_des[1]
        msg.base_vel.z = r_b_dot_des[2]

        msg.base_pos.x = r_b_des[0]
        msg.base_pos.y = r_b_des[1]
        msg.base_pos.z = r_b_des[2]

        msg.base_angvel.x = omega_des[0]
        msg.base_angvel.y = omega_des[1]
        msg.base_angvel.z = omega_des[2]

        msg.base_quat.x = q_des[0]
        msg.base_quat.y = q_des[1]
        msg.base_quat.z = q_des[2]
        msg.base_quat.w = q_des[3]

        msg.feet_acc = r_s_ddot_des.tolist()
        msg.feet_vel = r_s_dot_des.tolist()
        msg.feet_pos = r_s_des.tolist()

        msg.contact_feet = contactFeet

        self.publisher_.publish(msg)



def main(args=None):
    rclpy.init(args=args)

    minimal_publisher = MinimalPublisher()

    minimal_subscriber = MinimalSubscriber()

    thread = threading.Thread(target=rclpy.spin, args=(minimal_subscriber, ), daemon=True)
    thread.start()

    # Instantiate the motion planner
    planner = MotionPlanner()

    # Instantiate the fading filter class (used to filter the base acceleration)
    filter = FadingFilter()
    filter.order = 2
    filter.beta = 0.995


    # Set the rate of this node
    rate = minimal_subscriber.create_rate(int(1 / planner.dt))


    # ======================================================================== #

    # While cycle used to pause the execution of the script until the simulation is started and thus the measurements are obtained.
    q = minimal_subscriber.q_b
    a = minimal_subscriber.a_b
    while q.size == 0 or a.size == 0:
        q = minimal_subscriber.q_b
        a = minimal_subscriber.a_b

        rate.sleep()

    
    # ======================================================================== #

    # The time variable is used to enable the planner to have an initialization phase
    time = 0
    init_time = 0.5

    # Initial base position
    p_b_0 = minimal_subscriber.q_b[0:3]

    # Main loop
    try:
        while rclpy.ok():

            # Update the internal timer
            time += planner.dt

            # Get the base linear quantities (position, velocity and acceleration)
            p_b = minimal_subscriber.q_b[0:3]
            q_b = minimal_subscriber.q_b[3:7]
            v_b = minimal_subscriber.v_b[0:3]
            a_b_meas = minimal_subscriber.a_b

            # Rotate the acceleration in the inertial frame
            q_b_conj = np.array([-q_b[0], -q_b[1], -q_b[2], q_b[3]])
            a_b_quat = np.concatenate((a_b_meas, np.array([0])))
            a_b_meas_body = (q_mult(q_mult(q_b_conj, a_b_quat), q_b))[0:3]

            # Filter the acceleration measured with the 
            a_b = - filter.filter(a_b_meas_body, Ts=planner.dt)

            # Horizontal velocity command and yaw rate command
            vel_cmd = np.array([0.1,0])
            yaw_rate_cmd = 0

            # Perform a single iteration of the model predictive control
            if time > init_time:
                # Planner output after the initialization phase has finished
                contactFeet, r_b_ddot_des, r_b_dot_des, r_b_des, omega_des, q_des, r_s_ddot_des, r_s_dot_des, r_s_des = planner.mpc(p_b, v_b, a_b, vel_cmd, yaw_rate_cmd)
            else:
                contactFeet = ['LF', 'RF', 'LH', 'RH']

                # Base position quantities
                r_b_des, r_b_dot_des, r_b_ddot_des = planner._spline(np.array([p_b_0[0], p_b_0[1], 0.6]), np.array([p_b_0[0], p_b_0[1], planner.zcom]), time/init_time)

                # Base angular quantities
                omega_des = np.zeros(3)
                q_des = np.array([0., 0., 0., 1.])
            
                # Swing feet position quantities
                r_s_ddot_des = np.array([])
                r_s_dot_des = np.array([])
                r_s_des = np.array([])
            

            # Publish the desired generalized pose message
            minimal_publisher.publish_desired_gen_pose(
                contactFeet,
                r_b_ddot_des, r_b_dot_des, r_b_des,
                omega_des, q_des,
                r_s_ddot_des, r_s_dot_des, r_s_des
            )

            # Sleep for the remainder of time
            rate.sleep()
    except KeyboardInterrupt:
        pass
    

    minimal_publisher.destroy_node()
    minimal_subscriber.destroy_node()
    rclpy.shutdown()

    thread.join()



if __name__ == '__main__':
    main()