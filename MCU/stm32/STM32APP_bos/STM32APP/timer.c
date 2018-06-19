#include "timer.h"
#include "led.h"
#include "usart.h"
#include "control.h"
#include "can.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK战舰STM32开发板
//定时器 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2012/9/4
//版本：V1.1
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//********************************************************************************
//V1.1 20120904
//1,增加TIM3_PWM_Init函数。
//2,增加LED0_PWM_VAL宏定义，控制TIM3_CH2脉宽									  
//////////////////////////////////////////////////////////////////////////////////  
   	  
//通用定时器3中断初始化
//这里时钟选择为APB1的2倍，而APB1为36M
//arr：自动重装值。
//psc：时钟预分频数
//这里使用的是定时器3!
#define ENCODER_TIMER   TIM2  // Encoder unit connected to TIM2
u8 canbuf[8]={1},count=0;

// 电机母线电流  毫欧电阻 电压采样 值
//float ADC_Value;


static int queue_lenth =0;
	int fliter_speed;




void TIM3_Int_Init(void)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //时钟使能
	
	/*
	
	时间中断是72000000/71+1/1000=1000 HZ
	
	即1 ms
	  
	*/	
	//定时器TIM3初始化
	TIM_TimeBaseStructure.TIM_Period = 1000; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	
	TIM_TimeBaseStructure.TIM_Prescaler =71; //设置用来作为TIMx时钟频率除数的预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //根据指定的参数初始化TIMx的时间基数单位
 
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE ); //使能指定的TIM3中断,允许更新中断

// printf("\r\n enter 1\r\n");
	//中断优先级NVIC设置
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //先占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;  //从优先级3级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
//printf("\r\n enter 2\r\n");
	NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器
 //printf("\r\n enter 3\r\n");	
	TIM_Cmd(TIM3, ENABLE);  //使能TIMx		
	
}
//定时器3中断服务程序
void TIM3_IRQHandler(void)   //TIM3中断
{
	
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  //检查TIM3更新中断发生与否
		{
		  TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //清除TIMx更新中断标志 
		    
			ptForkLift->u16RunRecvPeriodMotor++;
			ptForkLift->u16RunSendPeriodMotor++;
			ptForkLift->u16RunPIDControl++;
			ptForkLift->u16RunDetADC++;//电机电流采样时间 计数
			
			if(ptForkLift->u16RunDetADC==25){// 25ms采集一次 电机电流电压 放入主程序了
				ptForkLift->u16RunDetADC=0;
				ADC_SoftwareStartConvCmd(ADC1,ENABLE);  // 使能ADC1 的软件转换功能  可以在主程序  开启功能 
			}
						
			if(ptForkLift->u16RunRecvPeriodMotor>10) // can 状态检测周期  10ms
			{
				ptForkLift->u16RunRecvPeriodMotor=1;
			}

			/*进行CAN离线状态与在线状态的判断*/
			ptForkLift->u16CanComPeriod++; //CAN 接收计数  时间周期
			
			if(ptForkLift->u16CanComPeriod==100)
			{
				ptForkLift->u16CanComCount[1]=ptForkLift->u16CanComCount[0];
			}
			
			else if(ptForkLift->u16CanComPeriod==500)
			{
			  
				if(ptForkLift->u16CanComCount[0]>ptForkLift->u16CanComCount[1])
					ptForkLift->bCanComBox = CanBoxPost; //CAN 收发正常
				else
					ptForkLift->bCanComBox = CanBoxPend; //CAN 收发有问题
				
				ptForkLift->u16CanComPeriod=0;
				ptForkLift->u16CanComCount[1]=ptForkLift->u16CanComCount[0]=0;
				LED0=!LED0;//提示系统正在运行
				//printf("EncoderSpeed %d\r\n",ptForkLift->s16EncoderSpeed);
			
			}
					
			if(ptForkLift->u16RunSendPeriodMotor == 50)// 50ms发送一次  移入 主程序 WHILE(1)中
			{
			
				ptForkLift->u16RunSendPeriodMotor = 0;
				ptForkLift->bCanSendCounter = ptForkLift->bCanSendCounter + 1;

				ptForkLift->Can_Txmessage.Data[0] = (ptForkLift->s32EncoderCounter)       & 0xff;
				ptForkLift->Can_Txmessage.Data[1] = ((ptForkLift->s32EncoderCounter)>>8)  & 0xff;
				ptForkLift->Can_Txmessage.Data[2] = ((ptForkLift->s32EncoderCounter)>>16) & 0xff;
				ptForkLift->Can_Txmessage.Data[3] = ((ptForkLift->s32EncoderCounter)>>24) & 0xff;
				ptForkLift->Can_Txmessage.Data[4] = (u8)(ptForkLift->bCanSendCounter)     & 0xff;
				ptForkLift->Can_Txmessage.Data[5] = ( ptForkLift->u16I_Value)             & 0xff;
				ptForkLift->Can_Txmessage.Data[6] = ((ptForkLift->u16I_Value)>>8)         & 0xff;	
				ptForkLift->s32EncoderCounter = 0;			
				Can_Send_Msg(); //包飞: 放到主程序中			
			}
			
			
			// 似乎没有
			if( ptForkLift->u16time_ms < 3000 )
				ptForkLift->u16time_ms ++;			
			if(ptForkLift->u16time_ms == 3000)
				ptForkLift->u16time_ms = 3001;
			
			
			// 也可以放入主程序中 
			if(ptForkLift->u16RunPIDControl == 25) // 这里可以设置 PID的 控制周期 单位 1ms  5 就是5ms
			{					
				//包飞:发现编码器的计数值是实际的4倍
				ptForkLift->s16EncoderSpeed=(ENCODER_TIMER->CNT-15000)/4 ;//采集编码器的速度

				ptForkLift->s32EncoderCounter = ptForkLift->s32EncoderCounter + (ENCODER_TIMER->CNT - 15000) / 4;

				ENCODER_TIMER->CNT = 15000;
				
			// printf("ES %d\r\n",ptForkLift->s16EncoderSpeed);
					
//			printf("s16EncoderSpeed %d\r\n",ptForkLift->s16EncoderSpeed);
//			printf(" s16speedwant %f\r\n",ptForkLift->s16speedwant);


				/**************串口调试信息******************/	
//			printf("\r\n\tENC\t");
//			printf("%f \r\n",ptForkLift->s16speedout);
//			printf("%d \r\n",ptForkLift->s16EncoderSpeed);
//			printf("%f \r\n",ptForkLift->s16speedwant);
				
				if(queue_lenth< QUEUE_LENGTH -1 ){  
					queue_lenth++;
					en_queue(ptmiddle_filter_queue,ptForkLift->s16EncoderSpeed);				 
				}
				else 
				{
					en_queue(ptmiddle_filter_queue,ptForkLift->s16EncoderSpeed);	
					de_queue(ptmiddle_filter_queue);					 
				}			
				fliter_speed = average_data(ptmiddle_filter_queue);
			  
				
//			  printf("\r\n\tENC_MEAN\t");
//			  printf("%d\r\n",fliter_speed);
				
//			  printf("\r\n\tVOL_SET_Point\t");
//			  printf("%f\r\n",ptForkLift->s16speedwant);

				
				POSITION_PID(ptForkLift,fliter_speed);
      //printf("FES %d\r\n",fliter_speed);
//				ZL_PID(ptForkLift,fliter_speed);
				
			 // 电机过流检测 
			 //ptForkLift->ADC_Value = (float)(ptForkLift->u32ADCResult)/4096*3.3;
//			 printf("\r\nMOTOR_CAR\t");
//			 printf("%f\r\n",ptForkLift->ADC_Value);
				
				ptForkLift->u16RunPIDControl = 0;
				
			}
			

			
			if ( ptForkLift->ADC_Value > 4.0)// 比较值根据 串口 ADC_Value 输出值设定  
			{
				//ptForkLift->s16speedwant = 0; // 设置 期望 速度为0 但是在 pid控制器 不稳定的 情况下没用
			 //  ptForkLift->u16PWM=0; 或者直接设定pwm输出为0 
			 //  ptForkLift->u16RunPIDControl = 0;  //关闭 pid 控制
			}
								
						
		}
}



//TIM1 PWM部分初始化 
//PWM输出初始化
//arr：自动重装值
//psc：时钟预分频数
void TIM1_PWM_Init()
{  
	//if(ptForkLift->u8CarID==1) //新版本   pA8 PA9均为pwm口
	//if(BOARD_version==1)
	//	{
	GPIO_InitTypeDef         GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef        TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);	//使能定时器1时钟
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA  | RCC_APB2Periph_AFIO, ENABLE);  //使能GPIO外设和AFIO复用功能模块时钟
	
	 
   //设置该引脚为复用输出功能,输出TIM1 CH1的PWM脉冲波形	GPIOA.8
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8; //TIM_CH1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO
		
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //TIM_CH2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO
	
	/*
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;//电机方向引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO
  GPIO_SetBits(GPIOA,GPIO_Pin_9);			//PA.9 输出高
	*/
	
	/*
	PWM频率=                    系统时钟                          72000000 
	            —————————————————————  = ————————————=20 KHz
					（TIM_Period+1）*（TIM_Prescaler+1）           1800*2
	
	
	*/
	
   //初始化TIM1
	TIM_TimeBaseStructure.TIM_Period = 1799;//1800*10*2-1;//1799; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_Prescaler =1; //设置用来作为TIMx时钟频率除数的预分频值 
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0; 
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure); //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位
	
	TIM_ARRPreloadConfig(TIM1, ENABLE);//启用ARR影子寄存器
	
	//初始化TIM1 Channel PWM模式	 
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //选择定时器模式:TIM脉冲宽度调制模式2
 	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //比较输出使能
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable; 
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; //输出极性:TIM输出比较极性高
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High; 
	TIM_OCInitStructure.TIM_Pulse = 0;//初始占空比为TIM_Pulse/TIM_Period
	
	
	// 通道1  PA8
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);           //根据T指定的参数初始化外设TIM1 OC1
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);  //使能TIM1在CCR1上的预装载寄存器
	
	/*新加入  将PA9口 更换为  pwm 口  之前为  普通IO口*/
	// 通道2  PA9	
 	TIM_OC2Init(TIM1, &TIM_OCInitStructure);           //根据T指定的参数初始化外设TIM1 OC2
	TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);  //使能TIM1在CCR2上的预装载寄存器
 
 
	TIM_Cmd(TIM1, ENABLE);  //使能TIM1
	TIM_CtrlPWMOutputs(TIM1, ENABLE);//使能PWM输出
	
		//printf("\r\n\t驱动板版本\t");
	  //printf("%d\r\n",ptForkLift->u8CarID);
		//printf("\r\n\t 新版本 \t");
		
	
 
	/*老版本 PA8  Pwm 口 PA9 普通IO口 确定方向*/
		
	//else if(ptForkLift->u8CarID==0){
	//else if(BOARD_version==0)	{
 /*	
	GPIO_InitTypeDef         GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef        TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);	//使能定时器1时钟
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA  | RCC_APB2Periph_AFIO, ENABLE);  //使能GPIO外设和AFIO复用重映射功能模块时钟
	
  //RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);
	
   //设置该引脚为复用输出功能,输出TIM1 CH1的PWM脉冲波形	GPIOA.8
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8; //TIM_CH1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出   即启用PWM功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO


	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;//电机方向引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO
  GPIO_ResetBits(GPIOA,GPIO_Pin_9);		  //PA.9 输出高

	
	
   //初始化TIM1
	TIM_TimeBaseStructure.TIM_Period = 1799; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_Prescaler =1; //设置用来作为TIMx时钟频率除数的预分频值 
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0; 
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure); //根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位
	
	TIM_ARRPreloadConfig(TIM1, ENABLE);//启用ARR影子寄存器
	
	//初始化TIM1 Channel PWM模式	 
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //选择定时器模式:TIM脉冲宽度调制模式2
 	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //比较输出使能
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable; 
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; //输出极性:TIM输出比较极性高
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High; 
	TIM_OCInitStructure.TIM_Pulse = 0;//初始占空比为TIM_Pulse/TIM_Period
	
	
	// 通道1  PA8
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);           //根据T指定的参数初始化外设TIM1 OC1
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);  //使能TIM1在CCR1上的预装载寄存器
	
	//新加入  将PA9口 更换为  pwm 口  之前为  普通IO口
	// 通道2  PA9	
 	//TIM_OC2Init(TIM1, &TIM_OCInitStructure);           //根据T指定的参数初始化外设TIM1 OC2
	//TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);  //使能TIM1在CCR2上的预装载寄存器
 
 
	TIM_Cmd(TIM1, ENABLE);  //使能TIM1
	TIM_CtrlPWMOutputs(TIM1, ENABLE);//使能PWM输出
	
		printf("\r\n\t驱动板版本\t");
	  printf("%d\r\n",ptForkLift->u8CarID);
		printf("\r\n\t 老版本 \t");
		*/
	//}
	
}

//TIM2 正交编码初始化
void TIM2_Encoder_Init(void)
{
	  GPIO_InitTypeDef         GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_ICInitTypeDef        TIM_ICInitStructure; 
	 NVIC_InitTypeDef          NVIC_InitStructure;
	
	    //PA0 ch1  ,PA1 ch2 
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);//使能TIM2时钟  
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);//使能GPIOA时钟

    GPIO_StructInit(&GPIO_InitStructure);//将GPIO_InitStruct中的参数按缺省值输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;         
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//PA0 PA1浮空输入  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);  
	
	
	/* 中断向量组初始化 Enable the TIM3 Update Interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
	
	
	TIM_DeInit(ENCODER_TIMER);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	
	//分频设置
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;  // No prescaling 
    TIM_TimeBaseStructure.TIM_Period = 30000;  
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;   
    TIM_TimeBaseInit(ENCODER_TIMER, &TIM_TimeBaseStructure);
	
	
	TIM_EncoderInterfaceConfig(ENCODER_TIMER, TIM_EncoderMode_TI12, 
                             TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 0;//选择输入比较滤波器
    TIM_ICInit(ENCODER_TIMER, &TIM_ICInitStructure);
  
    // Clear all pending interrupts 没有中断服务，所以此处都注释
    //TIM_ClearFlag(ENCODER_TIMER, TIM_FLAG_Update);
    //TIM_ITConfig(ENCODER_TIMER, TIM_IT_Update, ENABLE);//中断
	
    //Reset counter
    
	TIM_SetCounter(ENCODER_TIMER,15000);
	TIM_Cmd(ENCODER_TIMER, ENABLE);
	
	
	
}





