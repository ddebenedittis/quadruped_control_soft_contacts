#pragma once

#include "whole_body_controller/control_tasks.hpp"



namespace wbc {



/* ========================================================================== */
/*                           GENERALIZEDPOSE STRUCT                           */
/* ========================================================================== */

// TODO: this same struct is defined here and in static_walk_planner/static_walk_planner.hpp. Avoid this repetition.

/// @brief Stores the generalized desired pose computed by a planner and used by the whole-body controller.
struct GeneralizedPose {
    // Base linear quantities
    Eigen::Vector3d base_acc = {0, 0, 0};
    Eigen::Vector3d base_vel = {0, 0, 0};
    Eigen::Vector3d base_pos = {0, 0, 0};

    // Base angular quantities
    Eigen::Vector3d base_angvel = {0, 0, 0};
    Eigen::Vector4d base_quat = {0, 0, 0, 1};

    // Swing feet linear quantities
    Eigen::VectorXd feet_acc = {};
    Eigen::VectorXd feet_vel = {};
    Eigen::VectorXd feet_pos = {};

    // List of feet names in contact with the ground
    std::vector<std::string> contact_feet_names;
};



/* ========================================================================== */
/*                               TASKSNAMES ENUM                              */
/* ========================================================================== */

/// @brief Contains the names of the tasks defined in the ControlTasks class.
enum class TasksNames {
    FloatingBaseEOM,
    TorqueLimits,
    FrictionAndFcModulation,
    LinearBaseMotionTracking,
    AngularBaseMotionTracking,
    SwingFeetMotionTracking,
    ContactConstraints,
    EnergyAndForcesOptimization,
    SEPARATOR       // this should be the last element of the struct (used to count the number of control tasks too).
};



/* ========================================================================== */
/*                           PRIORITIZEDTASKS CLASS                           */
/* ========================================================================== */

/// @class @brief Implements the prioritized tasks, using the control tasks defined with the ControlTasks class component.
/// @details Provides methods to merge the control tasks generated by the ControlTasks class in a single task of priority p.
/// The priority order of the control tasks can be specified by means of the attribute prioritized_tasks_list.
class PrioritizedTasks {
    public:
        /// @brief Construct a new PrioritizedTasks class.
        PrioritizedTasks(const std::string& robot_name, float dt);

        void reset(
            const Eigen::VectorXd& q, const Eigen::VectorXd& v,
            const std::vector<std::string>& contact_feet_names);

        /// @brief Compute the matrices A, b, C, d that represents the task.
        /// @param[in] priority
        /// @param[out] A
        /// @param[out] b
        /// @param[out] C
        /// @param[out] d
        /// @param[in] gen_pose
        /// @param[in] d_k1
        /// @param[in] d_k2
        void compute_task_p(
            int priority,
            Eigen::MatrixXd& A, Eigen::VectorXd& b,
            Eigen::MatrixXd& C, Eigen::VectorXd& d,
            const GeneralizedPose& gen_pose,
            const Eigen::VectorXd& d_k1, const Eigen::VectorXd& d_k2
        );

        int get_nv() {return control_tasks.get_nv();}
        int get_nF() {return control_tasks.get_nF();}
        int get_nd() {return control_tasks.get_nd();}

        const Eigen::MatrixXd& get_M()  {return control_tasks.get_M();}
        const Eigen::VectorXd& get_h()  {return control_tasks.get_h();}
        const Eigen::MatrixXd& get_Jc() {return control_tasks.get_Jc();}

        const Eigen::VectorXd get_feet_position() { return control_tasks.get_feet_position(); }

        const std::vector<std::string>& get_generic_feet_names() const {return control_tasks.get_generic_feet_names();}

        const std::vector<std::string>& get_all_feet_names() const {return control_tasks.get_all_feet_names();}

        int get_max_priority() {return *max_element(tasks_vector.begin(), tasks_vector.end());}

        auto get_contact_constraint_type() {return this->contact_constraint_type;}
        void set_contact_constraint_type(ContactConstraintType contact_constraint_type) {this->contact_constraint_type = contact_constraint_type;}

        void set_tau_max(const double tau_max) {control_tasks.set_tau_max(tau_max);}
        void set_mu(const double mu) {control_tasks.set_mu(mu);}
        void set_Fn_max(const double Fn_max) {control_tasks.set_Fn_max(Fn_max);}
        void set_Fn_min(const double Fn_min) {control_tasks.set_Fn_min(Fn_min);}

        void set_kp_b_pos(const Eigen::Ref<const Eigen::Vector3d>& kp_b_pos) {control_tasks.set_kp_b_pos(kp_b_pos);}
        void set_kd_b_pos(const Eigen::Ref<const Eigen::Vector3d>& kd_b_pos) {control_tasks.set_kd_b_pos(kd_b_pos);}

        void set_kp_b_ang(const Eigen::Ref<const Eigen::Vector3d>& kp_b_ang) {control_tasks.set_kp_b_ang(kp_b_ang);}
        void set_kd_b_ang(const Eigen::Ref<const Eigen::Vector3d>& kd_b_ang) {control_tasks.set_kd_b_ang(kd_b_ang);}

        void set_kp_s_pos(const Eigen::Ref<const Eigen::Vector3d>& kp_s_pos) {control_tasks.set_kp_s_pos(kp_s_pos);}
        void set_kd_s_pos(const Eigen::Ref<const Eigen::Vector3d>& kd_s_pos) {control_tasks.set_kd_s_pos(kd_s_pos);}

        void set_kp_terr(const Eigen::Ref<const Eigen::Vector3d>& kp_terr) {control_tasks.set_kp_terr(kp_terr);}
        void set_kd_terr(const Eigen::Ref<const Eigen::Vector3d>& kd_terr) {control_tasks.set_kd_terr(kd_terr);}

    private:
        /// @brief Get the number of rows of the equality and inequality matrices (A and C) of a whole task of priority p (which may be composed by several elementary control tasks).
        std::vector<int> get_task_dimension(int priority);

        /// @brief Compute the vector that specifies the priority for each control task.
        void compute_prioritized_tasks_vector();

        /// @brief List of TaskNames used to specify the priority of every task.
        std::vector<TasksNames> prioritized_tasks_list = {
            TasksNames::FloatingBaseEOM, TasksNames::SEPARATOR,
            TasksNames::TorqueLimits, TasksNames::FrictionAndFcModulation, TasksNames::SEPARATOR,
            TasksNames::LinearBaseMotionTracking, TasksNames::AngularBaseMotionTracking, TasksNames::SwingFeetMotionTracking, TasksNames::SEPARATOR,
            TasksNames::ContactConstraints, TasksNames::SEPARATOR,
            TasksNames::EnergyAndForcesOptimization, TasksNames::SEPARATOR
        };

        ControlTasks control_tasks;

        /// @brief Auxiliary vector that is used to compute the various tasks matrices.
        std::vector<int> tasks_vector;

        /// @brief
        ContactConstraintType contact_constraint_type = ContactConstraintType::soft_kv;
};

} // namespace wbc