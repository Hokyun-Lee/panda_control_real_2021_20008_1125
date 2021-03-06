#include "controller.h"
#include <iostream>
#include <iomanip>
#include <cmath>

//FILE* fp1 = fopen("6-1.txt", "w");
FILE* fp2 = fopen("6-2-1.txt", "w");
FILE* fp3 = fopen("6-2-2.txt", "w");
FILE* fp4 = fopen("6-2-3.txt", "w");


void ArmController::compute()
{
    // Kinematics and dynamics calculation ------------------------------
    Eigen::Affine3d transform = model_interface_.getTransform(franka::Frame::kEndEffector, q_);
    Eigen::Affine3d transform2 = model_interface_.getTransform(franka::Frame::kJoint4, q_);
    x_ = transform.translation();
    rotation_ = transform.linear();
    j_ = model_interface_.getJacobianMatrix(franka::Frame::kEndEffector, q_);
    m_ = model_interface_.getMassMatrix(q_);
    m_inverse_ = m_.inverse();
    // For CLIK
    Eigen::Affine3d transform_from_q_desired = model_interface_.getTransform(franka::Frame::kEndEffector,
    q_desired_);
    x_from_q_desired_ = transform_from_q_desired.translation();
    rotation_from_q_desired_ = transform_from_q_desired.linear();
    j_from_q_desired_ = model_interface_.getJacobianMatrix(franka::Frame::kEndEffector, q_desired_);
    // -----------------------------------------------------

	x_dot_ = j_ * qdot_;
    
	if (is_mode_changed_)
	{
		is_mode_changed_ = false;
		write_ = false;
		write_init_ = true;

		control_start_time_ = play_time_;

		q_init_ = q_;
		qdot_init_ = qdot_;
		q_error_sum_.setZero();
		q_desired_ = q_;

		x_init_ = x_;
		x_init_2_ = x_2;
		x_dot_init_ = x_dot_.block<3,1>(0,0);
		x_cubic_old_ = x_;
		rotation_init_ = rotation_;
	}
	

	if (control_mode_ == "joint_ctrl_home")
	{
        double duration = 5.0;
		Vector7d target_position;
		target_position << 0.0, 0.0, 0.0, -M_PI / 4, 0.0, M_PI / 2, 0;
		q_desired_ = cubicVector<7>(play_time_, control_start_time_, control_start_time_ + duration, q_init_, target_position, qdot_init_, qdot_target_);
	}
	else if(control_mode_ == "joint_ctrl_init")
	{
        double duration = 5.0;
		Vector7d target_position;
		target_position << 0.0, 0.0, 0.0, -M_PI / 2., 0.0, M_PI / 2, 0;
		q_desired_ = cubicVector<7>(play_time_, control_start_time_, control_start_time_ + duration, q_init_, target_position, qdot_init_, qdot_target_);
	}
	else if (control_mode_ == "CLIK_init")
	{
		double duration = 5.0;
		Vector7d target_position;
		target_position << 0.0, 0.0, 0.0, -M_PI / 2., 0.0, M_PI / 2, 0;
		q_desired_ = cubicVector<7>(play_time_, control_start_time_, control_start_time_ + duration, q_init_, target_position, qdot_init_, qdot_target_);
	}
	else if (control_mode_ == "CLIK")
	{
		x_target_ << 0.25, 0.28, 0.65;
		rotation_target_ << 0.7071, 0.7071, 0, 0.7071, -0.7071, 0, 0, 0,-1;
       
		double duration = 5.0;

		AngleAxisd angle_axis_desired, angle_axis_target;

		Matrix3d rotation_desired = rotation_init_.transpose() * rotation_target_;
		angle_axis_target.fromRotationMatrix(rotation_desired);

		double angle_desired, angle_dot_desired;

		angle_desired = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, 0, angle_axis_target.angle(), 0, 0);
		angle_dot_desired = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, 0, angle_axis_target.angle(), 0, 0, hz_);
		
		Vector6d x_desired_, x_dot_desired_;
		Vector6d x_rot_from_q_desired_;

		x_desired_.head(3) = cubicVector<3>(play_time_, control_start_time_, control_start_time_+ duration, x_init_, x_target_, x_dot_init_, x_dot_target_);
		x_dot_desired_.head(3) = cubicDotVector<3>(play_time_, control_start_time_, control_start_time_ + duration, x_init_, x_target_, x_dot_init_, x_dot_target_,hz_);
		x_dot_desired_.tail<3>() = angle_dot_desired * angle_axis_target.axis();

		angle_axis_desired = AngleAxisd(angle_desired, angle_axis_target.axis());
		Matrix3d rotation_from_angle_axis = angle_axis_desired.toRotationMatrix();
		Matrix3d rotation_desired_zero = rotation_init_ * rotation_from_angle_axis;

		x_desired_.tail<3>() = DyrosMath::getPhi(rotation_desired_zero, rotation_from_q_desired_);
		x_rot_from_q_desired_.head(3) = x_from_q_desired_;
		x_rot_from_q_desired_.tail<3>() << 0, 0, 0;

		Matrix<double, 7, 6> pseudo_j_;
		pseudo_j_ = j_from_q_desired_.transpose() * (j_from_q_desired_ * j_from_q_desired_.transpose()).inverse();
		// cout << "pseudo_j_ :\n" << pseudo_j_ << endl;

		Matrix6d Kp;
		Kp.setIdentity();
		Kp = 1 * Kp;

		Vector7d q_dot_desired_;
		//cout << "xdesired error\n" << x_desired_ - x_q_desired_ << endl;
		q_dot_desired_ = pseudo_j_ * (x_dot_desired_ + Kp * (x_desired_ - x_rot_from_q_desired_));
		q_desired_ = q_desired_ + q_dot_desired_ *  dt_;
	}
    else if (control_mode_ == "Dynamics")
    {
        q_target_ << 0.0, 0.0, 0.0, -60*DEG2RAD, 0.0, 90*DEG2RAD, 0.0;
		
        // torque_desired_ = 
    }
	else if (control_mode_ == "Joint_Space_PD_init")
	{
		Vector7d target_q_;
		target_q_ << 0.0, 0.0, 0.0, -M_PI / 6.0, 0.0, M_PI / 2.0, 0.0;
		double duration = 5.0;
		Vector7d q_desired_dot_;
		for (int i = 0; i < 7; i++)
		{
			q_desired_(i) = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0);
			q_desired_dot_(i) = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0, hz_);
		}
		Matrix7d Kp;
		Matrix7d Kv;
		Kp = 225 * EYE(7);
		Kv = 30 * EYE(7);
		torque_desired_ = m_ * (Kp * (q_desired_ - q_) + Kv * (q_desired_dot_ - qdot_));
	}
	else if (control_mode_ == "Joint_Space_PD")
	{
		Vector7d target_q_;
		target_q_ << 0.0, 0.0, 0.0, -M_PI / 3.0, 0.0, M_PI / 2.0, 0.0;
		double duration = 5.0;
		Vector7d q_desired_dot_;
		for (int i = 0; i < 7; i++)
		{
			q_desired_(i) = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0);
			q_desired_dot_(i) = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0, hz_);
		}
		Matrix7d Kp;
		Matrix7d Kv;
		Kp = 225 * EYE(7);
		Kv = 30 * EYE(7);
		torque_desired_ = m_ * (Kp * (q_desired_ - q_) + Kv * (q_desired_dot_ - qdot_));

		fprintf(fp2, "%f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t \n", play_time_ - control_start_time_, q_desired_(0), q_desired_(1), q_desired_(2), q_desired_(3), q_desired_(4), q_desired_(5), q_desired_(6), q_(0), q_(1), q_(2), q_(3), q_(4), q_(5), q_(6));
	}
	else if (control_mode_ == "Task_Space_PD_init")
	{
		Vector7d target_q_;
		target_q_ << 0.0, 0.0, 0.0, -M_PI / 2.0, 0.0, M_PI / 2.0, 0.0;
		double duration = 5.0;
		Vector7d q_desired_dot_;
		for (int i = 0; i < 7; i++)
		{
			q_desired_(i) = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0);
			q_desired_dot_(i) = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0, hz_);
		}
		Matrix7d Kp;
		Matrix7d Kv;
		Kp = 225 * EYE(7);
		Kv = 30 * EYE(7);
		torque_desired_ = m_ * (Kp * (q_desired_ - q_) + Kv * (q_desired_dot_ - qdot_));
	}
	else if (control_mode_ == "Task_Space_PD")
	{
		double duration = 5.0;

		Matrix6d lambda_;
		lambda_ = (j_ * m_.inverse() * j_.transpose()).inverse();

		Matrix<double, 7, 6> j_bar_;
		j_bar_ = m_.inverse() * j_.transpose() * lambda_;

		x_target_(0) = x_init_(0);
		x_target_(1) = x_init_(1) + 0.1;
		x_target_(2) = x_init_(2);

		x_desired_dot_ << 0, 0, 0;

		for (int i = 0; i < 3; i++) {
			x_desired_(i) = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, x_init_(i), x_target_(i), 0, 0);
			x_desired_dot_(i) = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, x_init_(i), x_target_(i), 0, 0, hz_);
		}

		double Kp_val, Kv_val;
		Kp_val = 400;
		Kv_val = 40;

		Vector3d F_star_;
		F_star_ = Kp_val * EYE(3) * (x_desired_.head(3) - x_) + Kv_val * EYE(3) * (x_desired_dot_ - x_dot_.head(3));

		Vector3d M_star_;
		M_star_ = -Kp_val * EYE(3) * DyrosMath::getPhi(rotation_, rotation_init_) - Kv_val * x_dot_.tail<3>();

		Vector6d F_zero_star_;
		F_zero_star_ << F_star_, M_star_;

		Vector7d torque_zero_;
		torque_zero_ = m_ * (Kp_val * EYE(7) * (q_init_ - q_) - Kv_val * EYE(7) * qdot_);

		torque_desired_ = j_.transpose() * lambda_ * F_zero_star_ + (EYE(7) - j_.transpose() * j_bar_.transpose()) * torque_zero_;

		fprintf(fp3, "%f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t \n", play_time_ - control_start_time_, q_desired_(0), q_desired_(1), q_desired_(2), q_desired_(3), q_desired_(4), q_desired_(5), q_desired_(6), q_(0), q_(1), q_(2), q_(3), q_(4), q_(5), q_(6), x_desired_(0), x_desired_(1), x_desired_(2), x_(0), x_(1), x_(2));
	}
	else if (control_mode_ == "VelocitySaturation_init")
	{
		Vector7d target_q_;
		target_q_ << 0.0, -M_PI / 3.0, 0.0, -M_PI / 2.0, 0.0, M_PI / 6.0, 0.0;
		double duration = 5.0;
		Vector7d q_desired_dot_;
		for (int i = 0; i < 7; i++)
		{
			q_desired_(i) = DyrosMath::cubic(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0);
			q_desired_dot_(i) = DyrosMath::cubicDot(play_time_, control_start_time_, control_start_time_ + duration, q_init_(i), target_q_(i), 0, 0, hz_);
		}
		Matrix7d Kp;
		Matrix7d Kv;
		Kp = 225 * EYE(7);
		Kv = 30 * EYE(7);
		torque_desired_ = m_ * (Kp * (q_desired_ - q_) + Kv * (q_desired_dot_ - qdot_));
	}
	else if (control_mode_ == "VelocitySaturation")
	{
		double duration = 5.0;
		Matrix6d lambda_;
		// lambda_ = (j_ * m_.inverse() * j_.transpose() + 0.005 * EYE(6)).inverse();
		lambda_ = (j_ * m_.inverse() * j_.transpose()).inverse();

		Matrix<double, 7, 6> j_bar_;
		j_bar_ = m_.inverse() * j_.transpose() * lambda_;

		x_desired_(0) = 0.3;
		x_desired_(1) = -0.012;
		x_desired_(2) = 0.52;

		double Kp_val, Kv_val;
		Kp_val = 400.0;
		Kv_val = 40.0;

		
		for (int i = 0; i < 3; i++) {
			saturation_x_desired_dot(i) = Kp_val / Kv_val * (x_desired_(i) - x_(i));
			if (abs(saturation_x_desired_dot(i)) > 0.3) {
				saturation_x_desired_dot(i) = 0.3 / abs(x_desired_(i) - x_(i)) * (x_desired_(i) - x_(i));
			}
		}

		f_star = Kv_val * EYE(3) * (saturation_x_desired_dot - x_dot_.head(3));

		m_star = -Kp_val * EYE(3) * DyrosMath::getPhi(rotation_, rotation_init_) - Kv_val * x_dot_.tail<3>();

		Vector6d F_zero_star_;
		F_zero_star_ << f_star, m_star;

		Vector7d torque_zero_;
		torque_zero_ = m_ * (Kp_val * EYE(7) * (q_init_ - q_) - Kv_val * EYE(7) * qdot_);

		torque_desired_ = j_.transpose() * lambda_ * F_zero_star_ + (EYE(7) - j_.transpose() * j_bar_.transpose()) * torque_zero_;

		fprintf(fp4, "%f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t %f\t \n", play_time_ - control_start_time_, q_desired_(0), q_desired_(1), q_desired_(2), q_desired_(3), q_desired_(4), q_desired_(5), q_desired_(6), q_(0), q_(1), q_(2), q_(3), q_(4), q_(5), q_(6), x_desired_(0), x_desired_(1), x_desired_(2), x_(0), x_(1), x_(2));
	}
	else 
	{
		torque_desired_.setZero();
	}

	printState();

	tick_++;
}


void ArmController::printState()
{
	// TODO: Modify this method to debug your code

	static int DBG_CNT = 0;
	if (DBG_CNT++ > hz_ / 20.)
	{
		DBG_CNT = 0;

		cout << "q desired:\t";
		cout << std::fixed << std::setprecision(3) << q_desired_.transpose() << endl;
		cout << "q cubic:\t";
		cout << std::fixed << std::setprecision(3) << q_cubic_.transpose() << endl;
		cout << "q now    :\t";
		cout << std::fixed << std::setprecision(3) << q_.transpose() << endl;
		cout << "x desired:\t";
		cout << x_desired_.transpose() << endl;
		cout << "x_dot_:\t";
		cout << x_dot_.transpose() << endl;
		cout << "saturation_x_desired_dot:\t";
		cout << saturation_x_desired_dot.transpose() << endl;
		cout << "x        :\t";
		cout << x_.transpose() << endl;

	}

	if (play_time_ == control_start_time_ + 3)
	{
		cout << "---------------------------------------------------------------------" << endl;
		cout << "                     control time finished                           " << endl;
		cout << "---------------------------------------------------------------------" << endl;
	}
}



// Controller Core Methods ----------------------------

void ArmController::setMode(const std::string & mode)
{
	is_mode_changed_ = true;
	control_mode_ = mode;
	cout << "Current mode (changed) : " << mode << endl;
}
void ArmController::initDimension()
{
	dof_ = DOF;
	q_temp_.resize(DOF);
	j_temp_.resize(6, DOF);
	j_temp_2.resize(6, DOF);
	j_temp_2.setZero();

	qddot_.setZero();

	x_target_.setZero();
	q_desired_.setZero();
	qdot_target_.setZero();
	torque_desired_.setZero();
	x_dot_init_.setZero();
	x_dot_target_.setZero();

	g_temp_.resize(DOF);
	m_temp_.resize(DOF, DOF);

	Kp_.setZero();
	Kv_.setZero();
	Kp_joint_.setZero();
	Kv_joint_.setZero();
}

void ArmController::readData(const Vector7d &position, const Vector7d &velocity, const Vector7d &torque)
{
	for (size_t i = 0; i < dof_; i++)
	{
		q_(i) = position(i);
		qdot_(i) = velocity(i);
		torque_(i) = torque(i);
	}
}
void ArmController::readData(const Vector7d &position, const Vector7d &velocity)
{
	for (size_t i = 0; i < dof_; i++)
	{
		q_(i) = position(i);
		qdot_(i) = velocity(i);
		torque_(i) = 0;
	}
}

const Vector7d & ArmController::getDesiredPosition()
{
	return q_desired_;
}

const Vector7d & ArmController::getDesiredTorque()
{
	return torque_desired_;
}

void ArmController::closeFile()
{
	if(writeFile.is_open())
		writeFile.close();
}

void ArmController::initPosition(franka::RobotState state)
{
    q_init_ = q_;
    q_desired_ = q_init_;
    model_interface_.setRobotState(state);
}

void ArmController::updateTime(double dt)
{
    dt_ = dt;
    play_time_ += dt;
}
// ----------------------------------------------------

