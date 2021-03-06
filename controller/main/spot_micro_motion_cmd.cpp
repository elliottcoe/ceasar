
#include <memory>

#include "spot_micro_motion_cmd.h"
#include "spot_micro_kinematics/spot_micro_kinematics.h"
#include "spot_micro_idle.h"
#include "config.h"

#include <esp_log.h>
static char tag[] = "gait";

using namespace smk;

// Constructor
SpotMicroMotionCmd::SpotMicroMotionCmd()
{
	// Initialize Command
	cmd_ = Command();

	// Initialize state to Idle state
	state_ = std::make_unique<SpotMicroIdleState>();

	// Read in config parameters into smnc_
	readInConfigParameters();

	// Initialize spot micro kinematics object of this class
	sm_ = smk::SpotMicroKinematics(0.0f, 0.0f, 0.0f, smnc_.smc);

	// Set an initial body height and stance cmd for idle mode
	body_state_cmd_.euler_angs = {.phi = 0.0f, .theta = 0.0f, .psi = 0.0f};
	body_state_cmd_.xyz_pos = {.x = 0.0f, .y = smnc_.lie_down_height, .z = 0.0f};
	body_state_cmd_.leg_feet_pos = getLieDownStance();

	// Set the spot micro kinematics object to this initial command
	sm_.setBodyState(body_state_cmd_);

	// Initialize servo array message with 12 servo objects
	for (int i = 1; i <= smnc_.num_servos; i++)
	{
		servocontrol::Servo temp_servo;
		temp_servo.servo = i;
		temp_servo.value = 0;
		servo_array_absolute_.push_back(temp_servo);
	}
}

SpotMicroMotionCmd::~SpotMicroMotionCmd()
{
	// Free up the memory assigned from heap
}

void SpotMicroMotionCmd::readInConfigParameters()
{
	smnc_.debug_mode = true; // debug logs are configured with DEBUG_LEVEL on ESP config

	//# Robot structure parameters
	smnc_.smc.hip_link_length = HIP_LINK_LENGTH;
	smnc_.smc.upper_leg_link_length = UPPER_LEG_LINK_LENGTH;
	smnc_.smc.lower_leg_link_length = LOWER_LEG_LINK_LENGTH; //TODO: include foot length
	smnc_.smc.body_width = BODY_WIDTH;
	smnc_.smc.body_length = BODY_LENGTH;

	// Stance parameters,
	smnc_.default_stand_height = DEFAULT_STAND_HEIGHT;
	smnc_.stand_front_x_offset = STAND_FRONT_X_OFFSET;
	smnc_.stand_back_x_offset = STAND_BACL_X_OFFSET;
	smnc_.lie_down_height = LIE_DOWN_HEIGHT;
	smnc_.lie_down_feet_x_offset = LIE_DOWN_FEET_X_OFFSET;

	//# Control Parameters
	smnc_.transit_tau = TRANSI_TAU;
	smnc_.transit_rl = TRANSIT_RL;
	smnc_.transit_angle_rl = TRANSIT_ANGLE_RL;

	// # Gait parameters
	smnc_.max_fwd_velocity = MAX_FWD_VELOCITY;
	smnc_.max_side_velocity = MAX_SIDE_VELOCITY;
	smnc_.max_yaw_rate = MAX_YAW_RATE;
	smnc_.z_clearance = Z_CLEARANCE;
	smnc_.alpha = ALPHA;
	smnc_.beta = BETA;
	smnc_.num_phases = NUM_PHASES; // 8 phse Gait
	smnc_.rb_contact_phases = RB_CONTACT_PHASES;
	smnc_.rf_contact_phases = RF_CONTACT_PHASES;
	smnc_.lf_contact_phases = LF_CONTACT_PHASES;
	smnc_.lb_contact_phases = LB_CONTACT_PHASES;
	smnc_.overlap_time = OVERLAP_TIME;
	smnc_.swing_time = SWING_TIME;
	smnc_.foot_height_time_constant = FOOT_HEIGH_TIME_CONST;
	smnc_.body_shift_phases = BODY_SHIFT_PHASES;
	smnc_.fwd_body_balance_shift = FWD_BODY_BALANCE_SHIFT;
	smnc_.back_body_balance_shift = BACK_BODY_BALANCE_SHIFT;
	smnc_.side_body_balance_shift = SIDE_BODY_BALANCE_SHIFT;
	smnc_.dt = GAIT_FRAME_RATE;

	// Servo parameters
	smnc_.num_servos = 12;
	smnc_.servo_max_angle_deg = 82.5;

	// servo config map
	smnc_.servo_config["RF_3"] = {{"num", RF_LOWER_SERVO_CHANNEL}, {"center", RF_LOWER_SERVO_CENTER}, {"range", RF_LOWER_SERVO_RANGE}, {"direction", RF_LOWER_SERVO_DIRECTION}, {"center_angle_deg", RF_LOWER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["RF_2"] = {{"num", RF_UPPER_SERVO_CHANNEL}, {"center", RF_UPPER_SERVO_CENTER}, {"range", RF_UPPER_SERVO_RANGE}, {"direction", RF_UPPER_SERVO_DIRECTION}, {"center_angle_deg", RF_UPPER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["RF_1"] = {{"num", RF_HIP_SERVO_CHANNEL}, {"center", RF_HIP_SERVO_CENTER}, {"range", RF_HIP_SERVO_RANGE}, {"direction", RF_HIP_SERVO_DIRECTION}, {"center_angle_deg", RF_HIP_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["RB_3"] = {{"num", RB_LOWER_SERVO_CHANNEL}, {"center", RB_LOWER_SERVO_CENTER}, {"range", RB_LOWER_SERVO_RANGE}, {"direction", RB_LOWER_SERVO_DIRECTION}, {"center_angle_deg", RB_LOWER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["RB_2"] = {{"num", RB_UPPER_SERVO_CHANNEL}, {"center", RB_UPPER_SERVO_CENTER}, {"range", RB_UPPER_SERVO_RANGE}, {"direction", RB_UPPER_SERVO_DIRECTION}, {"center_angle_deg", RB_UPPER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["RB_1"] = {{"num", RB_HIP_SERVO_CHANNEL}, {"center", RB_HIP_SERVO_CENTER}, {"range", RB_HIP_SERVO_RANGE}, {"direction", RB_HIP_SERVO_DIRECTION}, {"center_angle_deg", RB_HIP_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LB_3"] = {{"num", LB_LOWER_SERVO_CHANNEL}, {"center", LB_LOWER_SERVO_CENTER}, {"range", LB_LOWER_SERVO_RANGE}, {"direction", LB_LOWER_SERVO_DIRECTION}, {"center_angle_deg", LB_LOWER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LB_2"] = {{"num", LB_UPPER_SERVO_CHANNEL}, {"center", LB_UPPER_SERVO_CENTER}, {"range", LB_UPPER_SERVO_RANGE}, {"direction", LB_UPPER_SERVO_DIRECTION}, {"center_angle_deg", LB_UPPER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LB_1"] = {{"num", LB_HIP_SERVO_CHANNEL}, {"center", LB_HIP_SERVO_CENTER}, {"range", LB_HIP_SERVO_RANGE}, {"direction", LB_HIP_SERVO_DIRECTION}, {"center_angle_deg", LB_HIP_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LF_3"] = {{"num", LF_LOWER_SERVO_CHANNEL}, {"center", LF_LOWER_SERVO_CENTER}, {"range", LF_LOWER_SERVO_RANGE}, {"direction", LF_LOWER_SERVO_DIRECTION}, {"center_angle_deg", LF_LOWER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LF_2"] = {{"num", LF_UPPER_SERVO_CHANNEL}, {"center", LF_UPPER_SERVO_CENTER}, {"range", LF_UPPER_SERVO_RANGE}, {"direction", LF_UPPER_SERVO_DIRECTION}, {"center_angle_deg", LF_UPPER_SERVO_CENTER_ANG_DEG}};
	smnc_.servo_config["LF_1"] = {{"num", LF_HIP_SERVO_CHANNEL}, {"center", LF_HIP_SERVO_CENTER}, {"range", LF_HIP_SERVO_RANGE}, {"direction", LF_HIP_SERVO_DIRECTION}, {"center_angle_deg", LF_HIP_SERVO_CENTER_ANG_DEG}};

	// Derived parameters
	// Round result of division of floats
	smnc_.overlap_ticks = round(smnc_.overlap_time / smnc_.dt);
	smnc_.swing_ticks = round(smnc_.swing_time / smnc_.dt);
	smnc_.stance_ticks = 7 * smnc_.swing_ticks;
	smnc_.overlap_ticks = round(smnc_.overlap_time / smnc_.dt);
	smnc_.phase_ticks = std::vector<int>{smnc_.swing_ticks, smnc_.swing_ticks, smnc_.swing_ticks, smnc_.swing_ticks,
										 smnc_.swing_ticks, smnc_.swing_ticks, smnc_.swing_ticks, smnc_.swing_ticks};
	smnc_.phase_length = smnc_.num_phases * smnc_.swing_ticks;
}

bool SpotMicroMotionCmd::init()
{
	// Create a temporary servo config
	servocontrol::ServoConfig temp_servo_config;
	std::map<int, servocontrol::ServoConfig> servo_config;

	// Loop through servo configuration dictionary in smnc_, append servo to
	int servo_i = 1; // servos numbered 1 -12
	for (std::map<std::string, std::map<std::string, float>>::iterator
			 iter = smnc_.servo_config.begin();
		 iter != smnc_.servo_config.end();
		 ++iter)
	{

		std::map<std::string, float> servo_config_params = iter->second;
		temp_servo_config.channel = servo_config_params["num"];
		temp_servo_config.center = servo_config_params["center"];
		temp_servo_config.range = servo_config_params["range"];
		temp_servo_config.direction = servo_config_params["direction"];

		// Append to temp_servo_config_array
		servo_config[servo_i] = temp_servo_config;
		servo_i++;
	}
	servocontrol::config(servo_config);
	return true;
}

SpotMicroNodeConfig SpotMicroMotionCmd::getNodeConfig()
{
	return smnc_;
}

LegsFootPos SpotMicroMotionCmd::getNeutralStance()
{
	float len = smnc_.smc.body_length;			 // body length
	float width = smnc_.smc.body_width;			 // body width
	float l1 = smnc_.smc.hip_link_length;		 // liength of the hip link
	float f_offset = smnc_.stand_front_x_offset; // x offset for front feet in neutral stance
	float b_offset = smnc_.stand_back_x_offset;	 // y offset for back feet in neutral stance

	LegsFootPos neutral_stance;
	neutral_stance.right_back = {.x = -len / 2 + b_offset, .y = 0.0f, .z = width / 2 + l1};
	neutral_stance.right_front = {.x = len / 2 + f_offset, .y = 0.0f, .z = width / 2 + l1};
	neutral_stance.left_front = {.x = len / 2 + f_offset, .y = 0.0f, .z = -width / 2 - l1};
	neutral_stance.left_back = {.x = -len / 2 + b_offset, .y = 0.0f, .z = -width / 2 - l1};

	return neutral_stance;
}

LegsFootPos SpotMicroMotionCmd::getLieDownStance()
{
	float len = smnc_.smc.body_length;	  // body length
	float width = smnc_.smc.body_width;	  // body width
	float l1 = smnc_.smc.hip_link_length; // length of the hip link
	float x_off = smnc_.lie_down_feet_x_offset;

	LegsFootPos lie_down_stance;
	lie_down_stance.right_back = {.x = -len / 2 + x_off, .y = 0.0f, .z = width / 2 + l1};
	lie_down_stance.right_front = {.x = len / 2 + x_off, .y = 0.0f, .z = width / 2 + l1};
	lie_down_stance.left_front = {.x = len / 2 + x_off, .y = 0.0f, .z = -width / 2 - l1};
	lie_down_stance.left_back = {.x = -len / 2 + x_off, .y = 0.0f, .z = -width / 2 - l1};

	return lie_down_stance;
}

void SpotMicroMotionCmd::changeState(std::unique_ptr<SpotMicroState> sms)
{
	// Change the active state
	state_ = std::move(sms);

	// Call init method of new state
	state_->init(sm_.getBodyState(), smnc_, cmd_, this);

	// Reset all command values
	cmd_.resetAllCommands();
}

void SpotMicroMotionCmd::setServoCommandMessageData()
{

	// Set the state of the spot micro kinematics object by setting the foot
	// positions, body position, and body orientation. Retrieve joint angles and
	// set the servo cmd message data
	sm_.setBodyState(body_state_cmd_);
	LegsJointAngles joint_angs = sm_.getLegsJointAngles();

	// Set the angles for the servo command message
	servo_cmds_rad_["RF_1"] = joint_angs.right_front.ang1;
	servo_cmds_rad_["RF_2"] = joint_angs.right_front.ang2;
	servo_cmds_rad_["RF_3"] = joint_angs.right_front.ang3;

	servo_cmds_rad_["RB_1"] = joint_angs.right_back.ang1;
	servo_cmds_rad_["RB_2"] = joint_angs.right_back.ang2;
	servo_cmds_rad_["RB_3"] = joint_angs.right_back.ang3;

	servo_cmds_rad_["LF_1"] = joint_angs.left_front.ang1;
	servo_cmds_rad_["LF_2"] = joint_angs.left_front.ang2;
	servo_cmds_rad_["LF_3"] = joint_angs.left_front.ang3;

	servo_cmds_rad_["LB_1"] = joint_angs.left_back.ang1;
	servo_cmds_rad_["LB_2"] = joint_angs.left_back.ang2;
	servo_cmds_rad_["LB_3"] = joint_angs.left_back.ang3;
}

void SpotMicroMotionCmd::publishZeroServoAbsoluteCommand()
{
	servocontrol::servos_absolute(servo_array_absolute_);
}

void SpotMicroMotionCmd::command_stand()
{
	cmd_.stand_cmd_ = true;
}

void SpotMicroMotionCmd::command_idle()
{
	cmd_.idle_cmd_ = true;
}

void SpotMicroMotionCmd::command_walk()
{
	cmd_.walk_cmd_ = true;
}

void SpotMicroMotionCmd::command_speed(float x, float y, float z)
{
	cmd_.x_vel_cmd_mps_ = x;
	cmd_.y_vel_cmd_mps_ = y;
	cmd_.yaw_rate_cmd_rps_ = z;
}

void SpotMicroMotionCmd::command_angle(float x, float y, float z)
{
	cmd_.phi_cmd_ = x;
	cmd_.theta_cmd_ = y;
	cmd_.psi_cmd_ = z;
}

void SpotMicroMotionCmd::runOnce()
{
	state_->handleInputCommands(sm_.getBodyState(), smnc_, cmd_, this, &body_state_cmd_);

	// Consume all event commands.
	cmd_.resetEventCmds();
}

void SpotMicroMotionCmd::publishServoProportionalCommand()
{
	// int servo_num;
	float cmd_ang_rad;
	float center_ang_rad;
	float servo_proportional_cmd;

	int servo_i = 1;
	for (std::map<std::string, std::map<std::string, float>>::iterator
			 iter = smnc_.servo_config.begin();
		 iter != smnc_.servo_config.end();
		 ++iter)
	{

		std::string servo_name = iter->first;
		std::map<std::string, float> servo_config_params = iter->second;

		// servo_num = servo_config_params["num"];
		cmd_ang_rad = servo_cmds_rad_[servo_name];
		center_ang_rad = servo_config_params["center_angle_deg"] * M_PI / 180.0f;
		servo_proportional_cmd = (cmd_ang_rad - center_ang_rad) /
								 (smnc_.servo_max_angle_deg * M_PI / 180.0f);

		if (servo_proportional_cmd > 1.0f)
		{
			servo_proportional_cmd = 1.0f;
			ESP_LOGD(tag, "Proportional Command above +1.0 was computed, clipped to 1.0");
			ESP_LOGD(tag, "Joint %s, Angle: %1.2f", servo_name.c_str(), cmd_ang_rad * 180.0 / M_PI);
		}
		else if (servo_proportional_cmd < -1.0f)
		{
			servo_proportional_cmd = -1.0f;
			ESP_LOGD(tag, "Proportional Command below -1.0 was computed, clipped to -1.0");
			ESP_LOGD(tag, "Joint %s, Angle: %1.2f", servo_name.c_str(), cmd_ang_rad * 180.0 / M_PI);
		}
		servocontrol::servos_proportional(servo_i, servo_proportional_cmd);

		servo_i++;
	}
}