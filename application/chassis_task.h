/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis.c/h
  * @brief      chassis control task,
  *             底盘控制任务
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. 完成
  *  V1.1.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#ifndef CHASSIS_TASK_H
#define CHASSIS_TASK_H
#include "struct_typedef.h"
#include "CAN_receive.h"
#include "gimbal_task.h"
#include "pid.h"
#include "remote_control.h"
#include "user_lib.h"

//in the beginning of task ,wait a time
//任务开始空闲一段时间
#define CHASSIS_TASK_INIT_TIME 357

//the channel num of controlling vertial speed 
//前后的遥控器通道号码
#define CHASSIS_X_CHANNEL 1
//the channel num of controlling horizontal speed
//左右的遥控器通道号码
#define CHASSIS_Y_CHANNEL 0

//in some mode, can use remote control to control rotation speed
//在特殊模式下，可以通过遥控器控制旋转
#define CHASSIS_WZ_CHANNEL 2

//the channel of choosing chassis mode,
//选择底盘状态 开关通道号
#define CHASSIS_MODE_CHANNEL 0
//rocker value (max 660) change to vertial speed (m/s) 
//遥控器前进摇杆（max 660）转化成车体前进速度（m/s）的比例
#define CHASSIS_VX_RC_SEN 0.006f
//rocker value (max 660) change to horizontal speed (m/s)
//遥控器左右摇杆（max 660）转化成车体左右速度（m/s）的比例
#define CHASSIS_VY_RC_SEN 0.005f
//in following yaw angle mode, rocker value add to angle 
//跟随底盘yaw模式下，遥控器的yaw遥杆（max 660）增加到车体角度的比例
#define CHASSIS_ANGLE_Z_RC_SEN 0.000002f
//in not following yaw angle mode, rocker value change to rotation speed
//不跟随云台的时候 遥控器的yaw遥杆（max 660）转化成车体旋转速度的比例
#define CHASSIS_WZ_RC_SEN 0.01f

#define CHASSIS_ACCEL_X_NUM 0.1666666667f
#define CHASSIS_ACCEL_Y_NUM 0.3333333333f

//rocker value deadline
//摇杆死区
#define CHASSIS_RC_DEADLINE 10
/* sin cos各个象限的45度 绝对值等于 omni wheel angle inverse kinematics coefficient-> / 0.707106781f
	 还需要*1/4 -> = 0.25f / 0.707106781f *  = 0.176776695f
							-> = 1.0f * 0.25f = 0.25f
*/
#define MOTOR_SPEED_TO_CHASSIS_SPEED_VX 0.25f //0.176776695f //0.25f
#define MOTOR_SPEED_TO_CHASSIS_SPEED_VY 0.25f //0.176776695f //0.25f
#define MOTOR_SPEED_TO_CHASSIS_SPEED_WZ 0.25f

/*
长=轴距: 285mm = 0.285m
宽=轮距: 420mm = 0.420m

0.285m + 0.420m = 0.705f 妈的算错了....

6-16-2022 SZL更正:
0.285m/2 + 0.420m/2 = 0.3525f

之前的数 0.2f

Single barrel infantry 2023 chassis version 1 2-10-2023
长=轴距: 365mm = 0.356m
宽=轮距: 360mm = 0.360m
0.356m/2 + 0.360m/2 = .178+.180 = 0.358 

3-22:
Omni Drive: 轮到中心250mm = 0.25m, 45度夹角
长=轴距: 0.25m * sqrt(2) = 0.353553391
宽=轮距: 0.25m * sqrt(2) = 0.353553391
0.353553391/2 + 0.353553391/2 = 0.353553391

6-18 Omni不是上面这样算的:
MOTOR_DISTANCE_TO_CENTER是车轮到旋转中心的距离 = 0.258093975f

4-21-2023 mini infantry
MOTOR_DISTANCE_TO_CENTER是车轮到旋转中心的距离 = 0.211585f
*/
#define MOTOR_DISTANCE_TO_CENTER 0.211585f //0.258093975f //0.353553391f //0.358f //0.3525f
#define OMNI_WHEEL_RADIUS 0.0762f
//sin cos各个象限的45度 绝对值等于 omni wheel angle inverse kinematics coefficient
#define OWHE_ANG_INVK_COEF 0.707106781f

//chassis task control time  2ms
//底盘任务控制间隔 2ms
#define CHASSIS_CONTROL_TIME_MS 2
//chassis task control time 0.002s
//底盘任务控制间隔 0.002s
#define CHASSIS_CONTROL_TIME 0.002f
//chassis control frequence, no use now.
//底盘任务控制频率，尚未使用这个宏
#define CHASSIS_CONTROL_FREQUENCE 500.0f
//chassis 3508 max motor control current
//底盘3508最大can发送电流值 16384-->20A
#define MAX_MOTOR_CAN_CURRENT 16000.0f
//press the key, chassis will swing
//底盘摇摆按键
#define SWING_KEY KEY_PRESSED_OFFSET_CTRL
//chassi forward, back, left, right key
//底盘前后左右控制按键
#define CHASSIS_FRONT_KEY KEY_PRESSED_OFFSET_W
#define CHASSIS_BACK_KEY KEY_PRESSED_OFFSET_S
#define CHASSIS_LEFT_KEY KEY_PRESSED_OFFSET_A
#define CHASSIS_RIGHT_KEY KEY_PRESSED_OFFSET_D

//m3508 rmp change to chassis speed,
//m3508转化成底盘速度(m/s)的比例，
#define M3508_MOTOR_RPM_TO_VECTOR 0.000415809748903494517209f
//#define CHASSIS_MOTOR_RPM_TO_VECTOR_SEN M3508_MOTOR_RPM_TO_VECTOR
/*
SZL 5-21-2022 重新算
2pi/60 * 1/19 * (0.084m Hex轮子半径) = 4.629715490e-4f

3-22: Omni wheel recalculation: 轮子直径d = 152mm, r = 152/2 = 76mm = 0.076m
2pi/60 * 1/19 * (0.076m Hex轮子半径) = 4.188790205e-4

4-21-2024: new mini infantry: 轮子直径d = 152.4mm, r = 152.4/2 = 76.2mm = 0.0762m 或 直径测量的 r = 0.08076m
2pi/60 * 1/14 * (0.0762m Hex轮子半径) = 5.699746672e-4
*/
#define CHASSIS_MOTOR_RPM_TO_VECTOR_SEN 5.699746672e-4

//single chassis motor max speed
//单个底盘电机最大速度
#define MAX_WHEEL_SPEED 8.0f 
//chassis forward or back max speed
//底盘运动过程最大前进速度
#define NORMAL_MAX_CHASSIS_SPEED_X 5.0f
//PR 底盘线性油门
#define TURBO_SPEED 3.5f //全油门模式速度
#define TURBO_ACC_STEP 0.05f //油门步进（加速度控制）
#define TURBO_INT_SPEED 0.18f //初始加速值（降低加速迟滞）

#define SLOW_SPEED 1.7f	//5-24-2022之前 1.4f //低速 模式速度
#define SLOW_ACC_STEP 0.015f //油门步进（加速度控制）
#define SLOW_INT_SPEED 0.1f //初始加速值（降低加速迟	滞）
#define SPIN_SPEED 8.0f //12.56f //6.66f //5.0f //4.0f //3.3f 3.0f //小陀螺速度
/*
8.0-76.39rpm; 8.3775-80rpm; 9.424777-90rpm

*/
/*
7-4-2023: 基于之前测试:
#define SPIN_SPEED 3.0f - 小陀螺时不会消耗太多功率
#define SPIN_SPEED 4.8f - 快很多
-------------------------------------------
原始的是: #define SPIN_SPEED 6.0f //小陀螺速度
//PR 底盘线性油门
#define TURBO_SPEED 2.8f //全油门模式速度
#define TURBO_ACC_STEP 0.029f //油门步进（加速度控制）
#define TURBO_INT_SPEED 0.2f //初始加速值（降低加速迟滞）

#define SLOW_SPEED 1.4f //低速 模式速度
#define SLOW_ACC_STEP 0.015f //油门步进（加速度控制）
#define SLOW_INT_SPEED 0.1f //初始加速值（降低加速迟	滞）
#define SPIN_SPEED 3.0f//6.0f //小陀螺速度
*/
//

/*原始参数: NORMAL_MAX_CHASSIS_SPEED_Y 3.0f; CHASSIS_WZ_SET_SCALE 0.1f*/
//chassis left or right max speed
//底盘运动过程最大平移速度
#define NORMAL_MAX_CHASSIS_SPEED_Y 2.3f

#define CHASSIS_WZ_SET_SCALE 0.1f

//when chassis is not set to move, swing max angle
//摇摆原地不动摇摆最大角度(rad)
#define SWING_NO_MOVE_ANGLE 0.7f
//when chassis is set to move, swing max angle
//摇摆过程底盘运动最大角度(rad)
#define SWING_MOVE_ANGLE 0.31415926535897932384626433832795f

//chassis motor speed PID
//底盘电机速度环PID
#define M3505_MOTOR_SPEED_PID_KP 9000.0f
#define M3505_MOTOR_SPEED_PID_KI 10.0f
#define M3505_MOTOR_SPEED_PID_KD 0.1f
#define M3505_MOTOR_SPEED_PID_MAX_OUT MAX_MOTOR_CAN_CURRENT
#define M3505_MOTOR_SPEED_PID_MAX_IOUT 2000.0f

/*
SZL 5-21-更改
#define CHASSIS_FOLLOW_GIMBAL_PID_KP 18.0f //40.0f  //Pr
#define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KD 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 10.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 0.2f
*/
//chassis follow angle PID
//底盘旋转跟随PID	
#define CHASSIS_FOLLOW_GIMBAL_PID_KP 5.5f//6.0f //40.0f  //Pr
#define CHASSIS_FOLLOW_GIMBAL_PID_KI 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_KD 0.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 10.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 0.2f

typedef enum
{
  CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW,   //chassis will follow yaw gimbal motor relative angle.底盘会跟随云台相对角度
  CHASSIS_VECTOR_FOLLOW_CHASSIS_YAW,  //chassis will have yaw angle(chassis_yaw) close-looped control.底盘有底盘角度控制闭环
	
	CHASSIS_VECTOR_SPIN, // chassis spining function. 底盘小陀螺 不跟随云台朝向
	
  CHASSIS_VECTOR_NO_FOLLOW_YAW,       //chassis will have rotation speed control. 底盘有旋转速度控制
  CHASSIS_VECTOR_RAW,                 //control-current will be sent to CAN bus derectly.

} chassis_mode_e;

typedef struct
{
  const motor_measure_t *chassis_motor_measure;
  fp32 accel;
  fp32 speed;
  fp32 speed_set;
  int16_t give_current;
} chassis_motor_t;

typedef struct
{
  const RC_ctrl_t *chassis_RC;               //底盘使用的遥控器指针, the point to remote control
  const gimbal_motor_t *chassis_yaw_motor;   //will use the relative angle of yaw gimbal motor to calculate the euler angle.底盘使用到yaw云台电机的相对角度来计算底盘的欧拉角.
  const gimbal_motor_t *chassis_pitch_motor; //will use the relative angle of pitch gimbal motor to calculate the euler angle.底盘使用到pitch云台电机的相对角度来计算底盘的欧拉角
  const fp32 *chassis_INS_angle;             //the point to the euler angle of gyro sensor.获取陀螺仪解算出的欧拉角指针
  chassis_mode_e chassis_mode;               //state machine. 底盘控制状态机
  chassis_mode_e last_chassis_mode;          //last state machine.底盘上次控制状态机
  chassis_motor_t motor_chassis[4];          //chassis motor data.底盘电机数据
  pid_type_def motor_speed_pid[4];             //motor speed PID.底盘电机速度pid
  pid_type_def chassis_angle_pid;              //follow angle PID.底盘跟随角度pid

  first_order_filter_type_t chassis_cmd_slow_set_vx;  //use first order filter to slow set-point.使用一阶低通滤波减缓设定值
  first_order_filter_type_t chassis_cmd_slow_set_vy;  //use first order filter to slow set-point.使用一阶低通滤波减缓设定值

  fp32 vx;                          //chassis vertical speed, positive means forward,unit m/s. 底盘速度 前进方向 前为正，单位 m/s
  fp32 vy;                          //chassis horizontal speed, positive means letf,unit m/s.底盘速度 左右方向 左为正  单位 m/s
  fp32 wz;                          //chassis rotation speed, positive means counterclockwise,unit rad/s.底盘旋转角速度，逆时针为正 单位 rad/s
  fp32 vx_set;                      //chassis set vertical speed,positive means forward,unit m/s.底盘设定速度 前进方向 前为正，单位 m/s
  fp32 vy_set;                      //chassis set horizontal speed,positive means left,unit m/s.底盘设定速度 左右方向 左为正，单位 m/s
  fp32 wz_set;                      //chassis set rotation speed,positive means counterclockwise,unit rad/s.底盘设定旋转角速度，逆时针为正 单位 rad/s
  fp32 chassis_relative_angle;      //the relative angle between chassis and gimbal.底盘与云台的相对角度，单位 rad
  fp32 chassis_relative_angle_set;  //the set relative angle.设置相对云台控制角度
  fp32 chassis_yaw_set;             

	//6-25-2023: 新增云台朝向的速度x y - 注意云台旋转是yaw rate, 底盘wz只管底盘
	fp32 vx_gimbal_orientation;
	fp32 vy_gimbal_orientation;

  fp32 vx_max_speed;  //max forward speed, unit m/s.前进方向最大速度 单位m/s
  fp32 vx_min_speed;  //max backward speed, unit m/s.后退方向最大速度 单位m/s
  fp32 vy_max_speed;  //max letf speed, unit m/s.左方向最大速度 单位m/s
  fp32 vy_min_speed;  //max right speed, unit m/s.右方向最大速度 单位m/s
  fp32 chassis_yaw;   //the yaw angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的yaw角度
  fp32 chassis_pitch; //the pitch angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的pitch角度
  fp32 chassis_roll;  //the roll angle calculated by gyro sensor and gimbal motor.陀螺仪和云台电机叠加的roll角度

} chassis_move_t;

/**
  * @brief          chassis task, osDelay CHASSIS_CONTROL_TIME_MS (2ms) 
  * @param[in]      pvParameters: null
  * @retval         none
  */
/**
  * @brief          底盘任务，间隔 CHASSIS_CONTROL_TIME_MS 2ms
  * @param[in]      pvParameters: 空
  * @retval         none
  */
extern void chassis_task(void const *pvParameters);

/**
  * @brief          accroding to the channel value of remote control, calculate chassis vertical and horizontal speed set-point
  *                 
  * @param[out]     vx_set: vertical speed set-point
  * @param[out]     vy_set: horizontal speed set-point
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" valiable point
  * @retval         none
  */
/**
  * @brief          根据遥控器通道值，计算纵向和横移速度
  *                 
  * @param[out]     vx_set: 纵向速度指针
  * @param[out]     vy_set: 横向速度指针
  * @param[out]     chassis_move_rc_to_vector: "chassis_move" 变量指针
  * @retval         none
  */
extern void chassis_rc_to_control_vector(fp32 *vx_set, fp32 *vy_set, chassis_move_t *chassis_move_rc_to_vector);

/*SZL 1-25 add get chassis_move pointer*/
extern const chassis_move_t *get_chassis_pointer(void);

extern uint8_t get_turboMode(void);
#endif
