#include "lip_walking_trot_planner/lip_walking_trot_planner_controller.hpp"

#include "lip_walking_trot_planner/quaternion_math.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"



namespace lip_walking_trot_planner {

using controller_interface::interface_configuration_type;
using controller_interface::InterfaceConfiguration;
using hardware_interface::HW_IF_POSITION;
using hardware_interface::HW_IF_VELOCITY;
using hardware_interface::HW_IF_EFFORT;

using namespace Eigen;


/* ================================= On_init ================================ */

CallbackReturn LIPController::on_init()
{
    try {
        auto_declare<double>("zero_time", double());
        auto_declare<double>("init_time", double());

        auto_declare<double>("sample_time", double());
        auto_declare<double>("step_duration", double());
        auto_declare<double>("step_height", double());

        auto_declare<int>("acc_filter_order", int());
        auto_declare<double>("acc_filter_beta", double());
    }
    catch(const std::exception& e) {
        fprintf(stderr,"Exception thrown during init stage with message: %s \n", e.what());
        return CallbackReturn::ERROR;
    }

    return CallbackReturn::SUCCESS;
}


/* ===================== Command_interface_configuration ==================== */

InterfaceConfiguration LIPController::command_interface_configuration() const
{
    InterfaceConfiguration command_interfaces_config;
    command_interfaces_config.type = interface_configuration_type::NONE;

    return command_interfaces_config;
}


/* ====================== State_interface_configuration ===================== */

InterfaceConfiguration LIPController::state_interface_configuration() const
{
    InterfaceConfiguration state_interfaces_config;
    state_interfaces_config.type = interface_configuration_type::NONE;

    return state_interfaces_config;
}


/* ============================== On_configure ============================== */

CallbackReturn LIPController::on_configure(const rclcpp_lifecycle::State& /*previous_state*/)
{
    zero_time_ = get_node()->get_parameter("zero_time").as_double();
    if (zero_time_ < 0) {
        RCLCPP_ERROR(get_node()->get_logger(),"'zero_time' parameter must be > 0.");
        return CallbackReturn::ERROR;
    }

    init_time_ = get_node()->get_parameter("init_time").as_double();
    if (init_time_ < 0) {
        RCLCPP_ERROR(get_node()->get_logger(),"'init_time' parameter must be > 0.");
        return CallbackReturn::ERROR;
    }


    double sample_time = get_node()->get_parameter("sample_time").as_double();
    if (planner_.set_sample_time(sample_time) == 1) {
        RCLCPP_ERROR(get_node()->get_logger(),"'sample_time' parameter must be > 0.");
        return CallbackReturn::ERROR;
    }


    std::string method = get_node()->get_parameter("interpolation_method").as_string();
    if (planner_.set_interpolation_method(method) == 1) {
        RCLCPP_ERROR(get_node()->get_logger(),"'interpolation_method' parameter is not an acceptable value.");
        return CallbackReturn::ERROR;
    }

    double step_duration = get_node()->get_parameter("step_duration").as_double();
    if (planner_.set_step_duration(step_duration) == 1) {
        RCLCPP_ERROR(get_node()->get_logger(),"'step_duration' parameter must be > 0.");
        return CallbackReturn::ERROR;
    }

    double step_height = get_node()->get_parameter("step_height").as_double();
    if (planner_.set_step_height(step_height) == 1) {
        RCLCPP_ERROR(get_node()->get_logger(),"'step_height' parameter must be > 0.");
        return CallbackReturn::ERROR;
    }

    double step_horizontal_phase_delay = get_node()->get_parameter("step_horizontal_phase_delay").as_double();
    if (planner_.set_step_horizontal_phase_delay(step_horizontal_phase_delay) == 1) {
        RCLCPP_ERROR(get_node()->get_logger(),"'step_horizontal_phase_delay' parameter must be >= 0 and < 1.");
        return CallbackReturn::ERROR;
    }

    planner_.set_foot_penetration(
        get_node()->get_parameter("foot_penetration").as_double()
    );
    

    /* ============================= Subscribers ============================ */

    feet_positions_subscription_ = get_node()->create_subscription<std_msgs::msg::Float64MultiArray>(
        "/logging/feet_positions", 1,
        [this](const std_msgs::msg::Float64MultiArray& msg) -> void
        {
            feet_positions.resize(4);

            for (int i = 0; i < 4; i++) {
                feet_positions[i] << msg.data[3*i + 0],
                                     msg.data[3*i + 1],
                                     msg.data[3*i + 2];
            }
        }
    );

    feet_velocities_subscription_ = get_node()->create_subscription<std_msgs::msg::Float64MultiArray>(
        "/logging/feet_velocities", 1,
        [this](const std_msgs::msg::Float64MultiArray& msg) -> void
        {
            feet_velocities.resize(4);

            for (int i = 0; i < 4; i++) {
                feet_velocities[i] << msg.data[3*i + 0],
                                      msg.data[3*i + 1],
                                      msg.data[3*i + 2];
            }
        }
    );

    imu_subscription_ = get_node()->create_subscription<sensor_msgs::msg::Imu>(
        "/imu_sensor_broadcaster/imu", 1,
        [this](const sensor_msgs::msg::Imu& msg) -> void
        {
            auto acc = msg.linear_acceleration;

            a_ << acc.x, acc.y, acc.z;
        }
    );

    link_states_subscription_ = get_node()->create_subscription<gazebo_msgs::msg::LinkStates>(
        "/gazebo/link_states", 1,
        [this](const gazebo_msgs::msg::LinkStates& msg) -> void
        {
            // The index of the base must be found by searching which link contains "base" in its name.
            int base_id = -1;

            for (int i = 0; i < static_cast<int>(msg.name.size()); i++) {
                if (msg.name[i].find("base") != std::string::npos) {
                    base_id = i;
                    break;
                }
            }
        
            // /gazebo/link_states returns the pose and the twist in the inertial or world frame.
        
            auto pos = msg.pose[base_id].position;
            auto quat = msg.pose[base_id].orientation;

            auto vel_lin = msg.twist[base_id].linear;
            auto vel_ang = msg.twist[base_id].angular;

            q_ << pos.x, pos.y, pos.z, quat.x, quat.y, quat.z, quat.w;
            v_ << vel_lin.x, vel_lin.y, vel_lin.z, vel_ang.x, vel_ang.y, vel_ang.z;
        }
    );
    
    terrain_penetrations_subscription_ = get_node()->create_subscription<std_msgs::msg::Float64MultiArray>(
        "state_estimator/terrain_penetration", 1,
        [this](const std_msgs::msg::Float64MultiArray& msg) -> void
        {
            terrain_penetrations_ << msg.data[0], msg.data[1], msg.data[2], msg.data[3];
        }
    );

    terrain_plane_subscription_ = get_node()->create_subscription<std_msgs::msg::Float64MultiArray>(
        "state_estimator/terrain_plane", 1,
        [this](const std_msgs::msg::Float64MultiArray& msg) -> void
        {
            plane_coeffs_ << msg.data[0], msg.data[1], msg.data[2];
        }
    );
    
    simple_velocity_command_subscription_ = get_node()->create_subscription<velocity_command_msgs::msg::SimpleVelocityCommand>(
        "/motion_generator/simple_velocity_command", 1,
        [this](const velocity_command_msgs::msg::SimpleVelocityCommand& msg) -> void
        {
            velocity_forward_ = msg.velocity_forward;
            velocity_lateral_ = msg.velocity_lateral;
            yaw_rate_ = msg.yaw_rate;
        }
    );

    /* ============================= Publishers ============================= */

    gen_pose_publisher_ = get_node()->create_publisher<generalized_pose_msgs::msg::GeneralizedPose>(
        "/motion_planner/desired_generalized_pose", rclcpp::SystemDefaultsQoS()
    );

    feet_trajectories_publisher_ = get_node()->create_publisher<rviz_legged_msgs::msg::Paths>(
        "rviz/feet_trajectory", rclcpp::SystemDefaultsQoS()
    );
    
    return CallbackReturn::SUCCESS;
}


/* =============================== On_activate ============================== */

CallbackReturn LIPController::on_activate(const rclcpp_lifecycle::State& /*previous_state*/)
{
    return CallbackReturn::SUCCESS;
}


/* ============================== On_deactivate ============================= */

CallbackReturn LIPController::on_deactivate(const rclcpp_lifecycle::State& /*previous_state*/)
{
    return CallbackReturn::SUCCESS;
}


/* ================================= Update ================================= */

controller_interface::return_type LIPController::update(const rclcpp::Time& time, const rclcpp::Duration& /*period*/)
{
    double time_double = static_cast<double>(time.seconds()) + static_cast<double>(time.nanoseconds()) * std::pow(10, -9);

    if (time_double > init_time_ + zero_time_) {
        Quaterniond quat_conj = Quaterniond(
            q_[6], q_[3], q_[4], q_[5]
        );

        Vector3d g = {0, 0, - 9.81};

        Vector3d a_b_meas_body = a_;
        quat_rot(quat_conj, a_b_meas_body);
        a_b_meas_body += g;
        // Vector3d a_b_meas_body = quat_conj * a_ + g;

        Vector3d a_b = - filter_.filter(a_b_meas_body, planner_.get_sample_time());

        gen_pose_ = planner_.update(
            q_.head(3), v_.head(3), a_b,
            (Vector2d() << velocity_forward_, velocity_lateral_).finished(), yaw_rate_,
            plane_coeffs_,
            feet_positions, feet_velocities
        );

        publish_feet_trajectories();
    } else if (time_double > zero_time_) {
        // Interpolate between the initial position and the starting position of the trot.

        Vector3d base_pos, base_vel, base_acc;

        double roll = std::atan(plane_coeffs_[1]);
        double pitch = - std::atan(plane_coeffs_[0]);
        double yaw = planner_.get_dtheta();

        Vector3d end_pos = init_pos_;
        end_pos[0] += planner_.get_height_com() * std::sin(pitch);
        end_pos[1] -= planner_.get_height_com() * std::sin(roll);
        end_pos[2] = planner_.get_height_com() * std::cos(roll) * std::cos(pitch) + plane_coeffs_[0] * q_[3] + plane_coeffs_[1] * q_[1] + plane_coeffs_[2];

        std::tie(base_pos, base_vel, base_acc) = MotionPlanner::spline(
            init_pos_, 
            end_pos,
            (time_double - zero_time_) / init_time_,
            InterpolationMethod::Spline_5th
        );

        gen_pose_.base_acc = generalized_pose::Vector3(base_acc);
        gen_pose_.base_vel = generalized_pose::Vector3(base_vel);
        gen_pose_.base_pos = generalized_pose::Vector3(base_pos);

        gen_pose_.base_angvel = generalized_pose::Vector3(0, 0, 0);

        Quaterniond quat = compute_quaternion_from_euler_angles(
            roll,
            pitch,
            yaw
        );
        gen_pose_.base_quat = generalized_pose::Quaternion(
            quat.x(), quat.y(), quat.z(), quat.w()
        );

    gen_pose_.base_quat = generalized_pose::Quaternion(
        quat.x(), quat.y(), quat.z(), quat.w()
    );
    } else {
        // Initialize the planner starting position and orientation.

        init_pos_ = q_.head(3);

        double dtheta = std::atan2(
            2 * (q_[6]*q_[5] + q_[3]*q_[4]), 
            1 - 2 * (q_[4]*q_[4] +  q_[5]*q_[5])
        );

        planner_.update_initial_conditions(init_pos_, dtheta, feet_positions);

        return controller_interface::return_type::OK;
    }

    gen_pose_publisher_->publish(gen_pose_.get_msg());

    return controller_interface::return_type::OK;
}


void LIPController::publish_feet_trajectories()
{
    auto trajectories = planner_.compute_trajectory_sample_points();

    auto msg = rviz_legged_msgs::msg::Paths();

    msg.header.frame_id = "ground_plane_link";

    int n_paths = trajectories.size();

    msg.paths.resize(n_paths);

    for (int i = 0; i < n_paths; i++) {
        int n_points = trajectories[i].size();

        msg.paths[i].poses.resize(n_points);

        for (int j = 0; j < n_points; j++) {
            msg.paths[i].poses[j].pose.position.x = trajectories[i][j][0];
            msg.paths[i].poses[j].pose.position.y = trajectories[i][j][1];
            msg.paths[i].poses[j].pose.position.z = trajectories[i][j][2];
        }
    }

    feet_trajectories_publisher_->publish(msg);
}

} // namespace lip_walking_trot_planner



PLUGINLIB_EXPORT_CLASS(
    lip_walking_trot_planner::LIPController,
    controller_interface::ControllerInterface)