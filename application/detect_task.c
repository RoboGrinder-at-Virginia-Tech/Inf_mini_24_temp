/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       detect_task.c/h
  * @brief      detect error task, judged by receiving data time. provide detect
                hook function, error exist function.
  *             检测错误任务， 通过接收数据时间来判断.提供 检测钩子函数,错误存在函数.
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. add oled, gyro accel and mag sensors
  *
  @verbatim
  ==============================================================================
    add a sensor 
    1. in detect_task.h, add the sensor name at the end of errorList,like
    enum errorList
    {
        ...
        XXX_TOE,    //new sensor
        ERROR_LIST_LENGHT,
    };
    2.in detect_init function, add the offlineTime, onlinetime, priority params,like
        uint16_t set_item[ERROR_LIST_LENGHT][3] =
        {
            ...
            {n,n,n}, //XX_TOE
        };
    3. if XXX_TOE has data_is_error_fun ,solve_lost_fun,solve_data_error_fun function, 
        please assign to function pointer.
    4. when XXX_TOE sensor data come, add the function detect_hook(XXX_TOE) function.
    如果要添加一个新设备
    1.第一步在detect_task.h，添加设备名字在errorList的最后，像
    enum errorList
    {
        ...
        XXX_TOE,    //新设备
        ERROR_LIST_LENGHT,
    };
    2.在detect_init函数,添加offlineTime, onlinetime, priority参数
        uint16_t set_item[ERROR_LIST_LENGHT][3] =
        {
            ...
            {n,n,n}, //XX_TOE
        };
    3.如果有data_is_error_fun ,solve_lost_fun,solve_data_error_fun函数，赋值到函数指针
    4.在XXX_TOE设备数据来的时候, 添加函数detect_hook(XXX_TOE).
  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
  
#include "detect_task.h"
#include "cmsis_os.h"

#include "SuperCap_comm.h"
#include "remote_control.h"
/**
  * @brief          init error_list, assign  offline_time, online_time, priority.
  * @param[in]      time: system time
  * @retval         none
  */
/**
  * @brief          初始化error_list,赋值 offline_time, online_time, priority
  * @param[in]      time:系统时间
  * @retval         none
  */
static void detect_init(uint32_t time);




error_t error_list[ERROR_LIST_LENGHT + 1];


#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t detect_task_stack;
#endif


/**
  * @brief          detect task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
/**
  * @brief          检测任务
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
void detect_task(void const *pvParameters)
{
    static uint32_t system_time;
    system_time = xTaskGetTickCount();
    //init,初始化
    detect_init(system_time);
    //wait a time.空闲一段时间
    vTaskDelay(DETECT_TASK_INIT_TIME);

    while (1)
    {
        static uint8_t error_num_display = 0;
        system_time = xTaskGetTickCount();

        error_num_display = ERROR_LIST_LENGHT;
        error_list[ERROR_LIST_LENGHT].is_lost = 0;
        error_list[ERROR_LIST_LENGHT].error_exist = 0;

        for (int i = 0; i < ERROR_LIST_LENGHT; i++)
        {
            //disable, continue
            //未使能，跳过
            if (error_list[i].enable == 0)
            {
                continue;
            }

            //judge offline.判断掉线 三种情况中的一种
            if (system_time - error_list[i].new_time > error_list[i].set_offline_time)
							{//SZL: 设备离线
                if (error_list[i].error_exist == 0)
                {
                    //record error and time
                    //记录错误以及掉线时间
                    error_list[i].is_lost = 1;
                    error_list[i].error_exist = 1;
                    error_list[i].lost_time = system_time;
                }
                //judge the priority,save the highest priority ,
                //判断错误优先级， 保存优先级最高的错误码
                if (error_list[i].priority > error_list[error_num_display].priority)
                {
                    error_num_display = i;
                }
                

                error_list[ERROR_LIST_LENGHT].is_lost = 1;
                error_list[ERROR_LIST_LENGHT].error_exist = 1;
                //if solve_lost_fun != NULL, run it
                //如果提供解决函数，运行解决函数
                if (error_list[i].solve_lost_fun != NULL)
                {
                    error_list[i].solve_lost_fun();
                }
            }
            else if (system_time - error_list[i].work_time < error_list[i].set_online_time) 
            {
								//SZL: 设备刚刚上线(设备离线后刚刚恢复); 
								//机器刚上电时的设备刚刚上线时, 刚刚完成detect_init 由于vTaskDelay(DETECT_TASK_INIT_TIME)的存在(此时中断也是可能发生的); 
								//所以 取决于set_online_time的大小, 机器刚上电时, 可能会或不会进入这个if
							
                //just online, maybe unstable, only record
                //刚刚上线，可能存在数据不稳定，只记录不丢失，
                error_list[i].is_lost = 0;
                error_list[i].error_exist = 1;
            }
            else
            {
							  //SZL:设备正常工作
                error_list[i].is_lost = 0;
                //判断是否存在数据错误
                //judge if exist data error
                if (error_list[i].data_is_error != NULL)
                {
                    error_list[i].error_exist = 1;
                }
                else
                {
                    error_list[i].error_exist = 0;
                }
                //calc frequency
                //计算频率
                if (error_list[i].new_time > error_list[i].last_time)
                {
                    error_list[i].frequency = configTICK_RATE_HZ / (fp32)(error_list[i].new_time - error_list[i].last_time);
                }
            }
        }

        vTaskDelay(DETECT_CONTROL_TIME);
#if INCLUDE_uxTaskGetStackHighWaterMark
        detect_task_stack = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}


/**
  * @brief          get toe error status
  * @param[in]      toe: table of equipment
  * @retval         true (eror) or false (no error)
  */
/**
  * @brief          获取设备对应的错误状态
  * @param[in]      toe:设备目录
  * @retval         true(错误) 或者false(没错误)
  */
bool_t toe_is_error(uint8_t toe)
{
    return (error_list[toe].error_exist == 1);
}

/**
  * @brief          record the time
  * @param[in]      toe: table of equipment
  * @retval         none
  */
/**
  * @brief          记录时间
  * @param[in]      toe:设备目录
  * @retval         none
  */
void detect_hook(uint8_t toe)
{
    error_list[toe].last_time = error_list[toe].new_time;
    error_list[toe].new_time = xTaskGetTickCount();
    
    if (error_list[toe].is_lost)
    {
        error_list[toe].is_lost = 0;
        error_list[toe].work_time = error_list[toe].new_time;
    }
    
    if (error_list[toe].data_is_error_fun != NULL)
    {
        if (error_list[toe].data_is_error_fun())
        {
            error_list[toe].error_exist = 1;
            error_list[toe].data_is_error = 1;

            if (error_list[toe].solve_data_error_fun != NULL)
            {
                error_list[toe].solve_data_error_fun();
            }
        }
        else
        {
            error_list[toe].data_is_error = 0;
        }
    }
    else
    {
        error_list[toe].data_is_error = 0;
    }
}

/**
  * @brief          get error list
  * @param[in]      none
  * @retval         the point of error_list
  */
/**
  * @brief          得到错误列表
  * @param[in]      none
  * @retval         error_list的指针
  */
const error_t *get_error_list_point(void)
{
    return error_list;
}

extern void OLED_com_reset(void);
extern void pc_offline_proc(void);
extern bool_t pc_is_data_error_proc(void);

static void detect_init(uint32_t time)
{
    //设置离线时间，上线稳定工作时间，优先级 offlineTime onlinetime priority
    uint16_t set_item[ERROR_LIST_LENGHT][3] =
        {
            {300, 40, 15},   //SBUS {30, 40, 15}
            {10, 10, 11},   //motor1
            {10, 10, 10},   //motor2
            {10, 10, 9},    //motor3
            {10, 10, 8},    //motor4
            {2, 3, 14},     //yaw
            {2, 3, 13},     //pitch - Left
						{2, 3, 13},     //pitch - Right
            {10, 10, 12},   //trigger Left 6-16先用的这个
						{10, 10, 12},   //trigger Right 6-16先用的这个
            {2, 3, 7},      //board gyro
            {5, 5, 7},      //board accel
            {40, 200, 7},   //board mag
            {100, 100, 5},  //referee
            {10, 10, 7},    //rm imu
            {100, 100, 6},  //oled {100, 100, 1}
						{200, 40, 15},   //MiniPC 串口通信 {200, 200, 15}
						{200, 40, 14},   //SuperCap SZL-3-12-2022 {200, 200, 14}
						{2000, 40, 14},   //sCap_23 2023新的超级电容 {2000, 100, 14}
						{3000, 40, 14},  //第三代超级电容
						{2000, 40, 14},		//雾列 超级电容 {2000, 2000, 14}
						{10, 10, 12},		//shooter L
						{10, 10, 12},		//shooter R
//						{10, 10, 12},		// - test compiler for out of bound
        };

    for (uint8_t i = 0; i < ERROR_LIST_LENGHT; i++)
    {
        error_list[i].set_offline_time = set_item[i][0];
        error_list[i].set_online_time = set_item[i][1];
        error_list[i].priority = set_item[i][2];
        error_list[i].data_is_error_fun = NULL;
        error_list[i].solve_lost_fun = NULL;
        error_list[i].solve_data_error_fun = NULL;

        error_list[i].enable = 1;
        error_list[i].error_exist = 1;
        error_list[i].is_lost = 1;
        error_list[i].data_is_error = 1;
        error_list[i].frequency = 0.0f;
        error_list[i].new_time = time;
        error_list[i].last_time = time;
        error_list[i].lost_time = time;
        error_list[i].work_time = time;
    }

    error_list[OLED_TOE].data_is_error_fun = NULL;
    error_list[OLED_TOE].solve_lost_fun = OLED_com_reset; //NULL; //OLED_com_reset;
    error_list[OLED_TOE].solve_data_error_fun = NULL;
		
		//SZL 3-12-2022
		error_list[ZIDACAP_TOE].data_is_error_fun = zidaCap_is_data_error_proc;
		error_list[ZIDACAP_TOE].solve_lost_fun = zidaCap_offline_proc;
		error_list[ZIDACAP_TOE].solve_data_error_fun = NULL;
		
		//SZL 12-27-2022 易林新超级电容
		error_list[GEN2CAP_TOE].data_is_error_fun = gen2Cap_is_data_error_proc;
		error_list[GEN2CAP_TOE].solve_lost_fun = gen2Cap_offline_proc;
		error_list[GEN2CAP_TOE].solve_data_error_fun = NULL;
		
		//SZL 3-19-2024 PR新超级电容
		error_list[GEN3CAP_TOE].data_is_error_fun = gen3Cap_is_data_error_proc;
		error_list[GEN3CAP_TOE].solve_lost_fun = gen3Cap_offline_proc;
		error_list[GEN3CAP_TOE].solve_data_error_fun = NULL;
		
		//雾列超级电容 相关函数
		error_list[WULIECAP_TOE].data_is_error_fun = wulieCap_is_data_error_proc;
		error_list[WULIECAP_TOE].solve_lost_fun = wulieCap_offline_proc;
		error_list[WULIECAP_TOE].solve_data_error_fun = NULL;
		
		//miniPC 相关掉线函数
		error_list[PC_TOE].data_is_error_fun = pc_is_data_error_proc; //miniPC_is_data_error_proc;
		error_list[PC_TOE].solve_lost_fun = pc_offline_proc;
		error_list[PC_TOE].solve_data_error_fun = NULL;

//    error_list[DBUSTOE].dataIsErrorFun = RC_data_is_error;
//    error_list[DBUSTOE].solveLostFun = slove_RC_lost;
//    error_list[DBUSTOE].solveDataErrorFun = slove_data_error;
//		error_list[DBUS_TOE].data_is_error_fun = RC_data_is_error;
//    error_list[DBUS_TOE].solve_lost_fun = slove_RC_lost;
//    error_list[DBUS_TOE].solve_data_error_fun = slove_data_error;

}
