#pragma once

#include "whole_body_controller/whole_body_controller.hpp"

#include "controller_interface/controller_interface.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "gazebo_msgs/msg/link_states.hpp"
#include "generalized_pose_msgs/msg/generalized_pose.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"

#include <string>
#include <vector>



namespace hqp_controller {

class HQPPublisher : public rclcpp::Node
{
    public:
        HQPPublisher();

        void publish_all(
            const Eigen::VectorXd& torques, const Eigen::VectorXd& forces,
            const Eigen::VectorXd& deformations, const Eigen::VectorXd& feet_position);

    private:
        rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr torques_publisher_;
        rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr forces_publisher_;
        rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr deformations_publisher_;

        rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr feet_position_publisher_;
};

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class HQPController : public controller_interface::ControllerInterface {
    public:
        HQPController();

        controller_interface::InterfaceConfiguration command_interface_configuration() const override;
        controller_interface::InterfaceConfiguration state_interface_configuration() const override;

        controller_interface::return_type update(
            const rclcpp::Time& time, const rclcpp::Duration& /*period*/
        ) override;

        CallbackReturn on_init() override;
        CallbackReturn on_configure(const rclcpp_lifecycle::State& /*previous_state*/) override;
        CallbackReturn on_activate(const rclcpp_lifecycle::State& /*previous_state*/) override;
        CallbackReturn on_deactivate(const rclcpp_lifecycle::State& /*previous_state*/) override;
        // CallbackReturn on_cleanup(const rclcpp_lifecycle::State& previous_state) override;
        // CallbackReturn on_error(const rclcpp_lifecycle::State& previous_state) override;
        // CallbackReturn on_shutdown(const rclcpp_lifecycle::State& previous_state) override;



    protected:
        wbc::WholeBodyController wbc;

        std::vector<std::string> joint_names_;

        Eigen::VectorXd q_;
        Eigen::VectorXd v_;
        wbc::GeneralizedPose des_gen_pose_;

        Eigen::VectorXd tau_;

        double PD_proportional_;
        double PD_derivative_;

        rclcpp::Subscription<gazebo_msgs::msg::LinkStates>::SharedPtr joint_state_subscription_ = nullptr;

        rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr estimated_pose_subscription_ = nullptr;
        rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr estimated_twist_subscription_ = nullptr;

        rclcpp::Subscription<generalized_pose_msgs::msg::GeneralizedPose>::SharedPtr desired_generalized_pose_subscription_ = nullptr;

        /// @brief If true, the controller will publish the computed optimal joint torques, contact forces, and feet deformations.
        bool logging_ = false;

        std::shared_ptr<HQPPublisher> logger_ = nullptr;

        /// @brief Initialization time to give the state estimator some time to get better estimates. During this time, a PD controller is used to keep the robot in q0 and the planner is paused.
        double init_time_ = 1;
};

} // namespace hqp_controller