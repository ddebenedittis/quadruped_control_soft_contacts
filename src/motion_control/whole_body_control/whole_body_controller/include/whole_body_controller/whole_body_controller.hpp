#pragma once

#include "hierarchical_optimization/hierarchical_qp.hpp"
#include "whole_body_controller/deformations_history_manager.hpp"
#include "whole_body_controller/prioritized_tasks.hpp"



namespace wbc {

/// @class @brief 
class WholeBodyController {
    public:
        WholeBodyController(std::string robot_name, float dt);

        ///@brief 
        ///
        ///@param q 
        ///@param v 
        ///@param gen_pose 
        void step(const Eigen::VectorXd& q, const Eigen::VectorXd& v, const GeneralizedPose& gen_pose);

        Eigen::VectorXd& get_x_opt() {return x_opt;}

        Eigen::VectorXd& get_tau_opt() {return tau_opt;}

        Eigen::VectorXd& get_f_c_opt() {return f_c_opt;}

        Eigen::VectorXd& get_d_des_opt() {return d_des_opt;}

        const Eigen::VectorXd get_feet_position() { return prioritized_tasks.get_feet_position(); }

        const std::vector<std::string>& get_generic_feet_names() const {return prioritized_tasks.get_generic_feet_names();}

        const std::vector<std::string>& get_all_feet_names() {return prioritized_tasks.get_all_feet_names();}

        int get_nv() {return prioritized_tasks.get_nv();}

        void set_contact_constraint_type(const std::string& contact_constraint_type)
        {
            if (contact_constraint_type == "soft_kv") {
                this->prioritized_tasks.set_contact_constraint_type(ContactConstraintType::soft_kv);
            } else if (contact_constraint_type == "rigid") {
                this->prioritized_tasks.set_contact_constraint_type(ContactConstraintType::rigid);
            } else {
                this->prioritized_tasks.set_contact_constraint_type(ContactConstraintType::invalid);
            }
        }

        void set_tau_max(const double tau_max) {prioritized_tasks.set_tau_max(tau_max);}
        void set_mu(const double mu) {prioritized_tasks.set_mu(mu);}
        void set_Fn_max(const double Fn_max) {prioritized_tasks.set_Fn_max(Fn_max);}
        void set_Fn_min(const double Fn_min) {prioritized_tasks.set_Fn_min(Fn_min);}

        void set_kp_b_pos(const Eigen::Ref<const Eigen::Vector3d>& kp_b_pos) {prioritized_tasks.set_kp_b_pos(kp_b_pos);}
        void set_kd_b_pos(const Eigen::Ref<const Eigen::Vector3d>& kd_b_pos) {prioritized_tasks.set_kd_b_pos(kd_b_pos);}

        void set_kp_b_ang(const Eigen::Ref<const Eigen::Vector3d>& kp_b_ang) {prioritized_tasks.set_kp_b_ang(kp_b_ang);}
        void set_kd_b_ang(const Eigen::Ref<const Eigen::Vector3d>& kd_b_ang) {prioritized_tasks.set_kd_b_ang(kd_b_ang);}

        void set_kp_s_pos(const Eigen::Ref<const Eigen::Vector3d>& kp_s_pos) {prioritized_tasks.set_kp_s_pos(kp_s_pos);}
        void set_kd_s_pos(const Eigen::Ref<const Eigen::Vector3d>& kd_s_pos) {prioritized_tasks.set_kd_s_pos(kd_s_pos);}

        void set_kp_terr(const Eigen::Ref<const Eigen::Vector3d>& kp_terr) {prioritized_tasks.set_kp_terr(kp_terr);}
        void set_kd_terr(const Eigen::Ref<const Eigen::Vector3d>& kd_terr) {prioritized_tasks.set_kd_terr(kd_terr);}

    private:
        void compute_torques();

        PrioritizedTasks prioritized_tasks;

        DeformationsHistoryManager deformations_history_manager;

        hopt::HierarchicalQP hierarchical_qp;

        Eigen::VectorXd x_opt;      /// @brief Optimal optimization vector

        Eigen::VectorXd tau_opt;    /// @brief Optimal joint torques

        Eigen::VectorXd f_c_opt;    /// @brief Optimal contact forces

        Eigen::VectorXd d_des_opt;  /// @brief Optimal desired feet deformations

        ContactConstraintType contact_constraint_type;
};

}