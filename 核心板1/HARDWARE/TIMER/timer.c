#include "led.h"
#include "adc.h"
#include "lcd.h"
#include "dac.h"
#include "timer.h"
#include "usart.h"
#include "delay.h"
#include "arm_math.h"
#include "bsp_FFT.h"
#include "stm32_dsp.h"

#define MAX(A,B) ((A > B) ? (A) : (B))

float filter_list[23] = {90,110,134, 164, 201, 245, 300, 367, 448, 548, 669, 818, 1000, 1222, 1494, 1826, 2232, 2728, 3334, 4075, 4980, 6087, 7440 };


int32_t lBufInArray[NPT];
int32_t lBufOutArray[NPT];
float lBufMagArray[NPT];

#define numStages  2               /* 2��IIR�˲��ĸ��� */
#define TEST_LENGTH_SAMPLES  1    /* �˲���ÿ�β������� */


arm_biquad_casd_df1_inst_f32 S;
static float testInput_f32[TEST_LENGTH_SAMPLES];            /* ������ */
static float testOutput[TEST_LENGTH_SAMPLES];               /* �˲������� */


//const float ScaleValue[1] = {0.7552019639411458f};


	
//const float IIRCoeffs32LP[5*numStages] = {
//1.0f,  -1.8693825150758503f,  1.0f,    1.8218452247513464f,  -0.98685832391078687f,
//1.0f,  -1.9390053706493142f,  1.0f,    1.9453160862541781f,  -0.99282938459074133f,
//1.0f,  -1.8849194398603f,     1.0f,    1.7070852391494f ,    -0.90723103295422347f,
//1.0f,  -1.9306401708916825f,  1.0f,    1.9222360460931638f,  -0.95887136900486325f,
//1.0f,  -1.9105923088862271f,  1.0f,    1.3852641979374472f,  -0.45008874106164687f};


	
	
/*����ϵ�� */
//const float ScaleValue[27] = { 0.9774287619394793, 0.9759506945346094, 0.9737397160098815, 0.9711694799152676, 0.9686091341723748, 0.9646055682610286, 0.9767680435184323, 0.9697052181058162, 0.8941541631812163, 0.8803075114427087, 0.8648909310940786, 0.846517939674481, 0.8263360883135138, 0.7848774876934997, 0.7574624223892135, 0.7273235163514175, 0.6935728730320836, 0.656103215086204, 0.6150446878024517, 0.5703851297893177, 0.6759745683848492, 0.6233274757867211, 0.15333673612545334, 0.11253267790615341, 0.07749720857064603, 0.14617859538825265, 0.18859533517655092};
const float ScaleValue[1] = { 
0.82786539706138496 *                                                  
0.82786539706138496  };


//ͨ�ö�ʱ���жϳ�ʼ��
//����ʱ��ѡ��ΪAPB1��2������APB1Ϊ36M
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��
//����ʹ�õ��Ƕ�ʱ��3!


void InitBufInArray()
{
    unsigned short i;
    float fx;
    for(i=0; i<NPT; i++)
    {
        fx = 1.50 * sin(PI2 * i * 350.0 / Fs) +
             2.70 * sin(PI2 * i * 8400.0 / Fs) +
             4.00 * sin(PI2 * i * 18725.0 / Fs);
        lBufInArray[i] = ((signed short)(fx*1000)) << 16;
    }
}


void TIM3_Int_Init(u16 arr,u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //ʱ��ʹ��

	TIM_TimeBaseStructure.TIM_Period = arr; //��������һ�������¼�װ�����Զ���װ�ؼĴ������ڵ�ֵ	 ������5000Ϊ500ms
	TIM_TimeBaseStructure.TIM_Prescaler = psc; //����������ΪTIMxʱ��Ƶ�ʳ�����Ԥ��Ƶֵ  10Khz�ļ���Ƶ��  
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //����ʱ�ӷָ�:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM���ϼ���ģʽ
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //����TIM_TimeBaseInitStruct��ָ���Ĳ�����ʼ��TIMx��ʱ�������λ
 
	TIM_ITConfig(  //ʹ�ܻ���ʧ��ָ����TIM�ж�
		TIM3, //TIM2
		TIM_IT_Update ,
		ENABLE  //ʹ��
		);
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3�ж�
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //��ռ���ȼ�0��
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;  //�����ȼ�3��
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQͨ����ʹ��
	NVIC_Init(&NVIC_InitStructure);  //����NVIC_InitStruct��ָ���Ĳ�����ʼ������NVIC�Ĵ���

	TIM_Cmd(TIM3, ENABLE);  //ʹ��TIMx����
							 
}

void TIM5_Int_Init(u16 arr,u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE); //ʱ��ʹ��

	TIM_TimeBaseStructure.TIM_Period = arr; //��������һ�������¼�װ�����Զ���װ�ؼĴ������ڵ�ֵ	 ������5000Ϊ500ms
	TIM_TimeBaseStructure.TIM_Prescaler = psc; //����������ΪTIMxʱ��Ƶ�ʳ�����Ԥ��Ƶֵ  10Khz�ļ���Ƶ��  
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //����ʱ�ӷָ�:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM���ϼ���ģʽ
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure); //����TIM_TimeBaseInitStruct��ָ���Ĳ�����ʼ��TIMx��ʱ�������λ
 
	TIM_ITConfig(  //ʹ�ܻ���ʧ��ָ����TIM�ж�
		TIM5, //TIM2
		TIM_IT_Update ,
		ENABLE  //ʹ��
		);
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;  //TIM3�ж�
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //��ռ���ȼ�0��
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 4;  //�����ȼ�3��
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQͨ����ʹ��
	NVIC_Init(&NVIC_InitStructure);  //����NVIC_InitStruct��ָ���Ĳ�����ʼ������NVIC�Ĵ���

	TIM_Cmd(TIM5, ENABLE);  //ʹ��TIMx����
							 
}

void LCD_display(u16 x,u16 y,float num)
{
	  u16 temp;
		temp=num;
		LCD_ShowxNum(x,y,temp,1,16,0);//��ʾ��ѹֵ
		num-=temp;
		num*=1000;
		LCD_ShowxNum(x+16,y,num,3,16,0X80);
}

void TIM3_IRQHandler(void)   //TIM3�ж�
{
	float temp;
	static int i = 0;
	int k = 0 , j = 0, m = 0;
	u16 adczu[NPT];
	float fft_num=0;
	float temp_max = 0.0;
	float fft_max=0.0;
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) //���ָ����TIM�жϷ������:TIM �ж�Դ 
		{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);  //���TIMx���жϴ�����λ:TIM �ж�Դ 
		LED1=!LED1;
		adczu[i]=Get_Adc(ADC_Channel_1);
		//LCD_display(156,100,temp);

		if (i == 1023)
			{
				i = -1;
				TIM_Cmd(TIM3, DISABLE);
				
				for(k=1;k<NPT;k++)
				{
					temp=(((float)adczu[k]*(3.3/4096)-1.5)*10/3);
					lBufInArray[k] = ((signed short)(temp*1000)) << 16;
				}

				//InitBufInArray();
				cr4_fft_1024_stm32(lBufOutArray,lBufInArray,NPT);    
				GetPowerMag();
				
				
				fft_max=lBufMagArray[1];
				
				POINT_COLOR=RED;
				
				
				for(j=1;j<NPT/2;j++)
				{
					temp_max=lBufInArray[j];
					if(lBufMagArray[j]>=fft_max)
					{
						
						fft_max=lBufMagArray[j];
						fft_num=j*39.0625;
						LCD_Fill(20,16,280,208,WHITE);
						LCD_ShowxNum(156,100,fft_max,5,16,0);
						LCD_ShowxNum(156,130,temp_max,5,16,0);
						LCD_ShowxNum(156,190,fft_num,5,16,0);
						fft_num=j;
					}
				}
				
				for(m=1;m<NPT/4;m++)
				{
					POINT_COLOR=RED;
					LCD_DrawLine(m - 1 + 20, (int)(208-0.038*MAX(lBufMagArray[2*m-1],lBufMagArray[2*m])), m -1 + 20, 208);
				}	
			}
		i += 1;
		}
}





void TIM5_IRQHandler(void)   //TIM3�ж�
{
	float temp;
	static int i = 0;
	int k = 0 , j = 0, m = 0;
	u16 adczu[NPT];
	float filter=0;
	int filter_num=0;
	float temp_max = 0.0;
	float fft_num = 0;
	float fft_max2 = 0.0;
	float fft_max=0.0;
	uint16_t ReadValue; 
	if (TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET) //���ָ����TIM�жϷ������:TIM �ж�Դ 
		{
		TIM_ClearITPendingBit(TIM5, TIM_IT_Update);  //���TIMx���жϴ�����λ:TIM �ж�Դ 
		LED1=!LED1;
		adczu[i]=Get_Adc(ADC_Channel_1);

		if (i == 1023)
			{
				i = -1;
				TIM_Cmd(TIM5, DISABLE);
				
				for(k=1;k<NPT;k++)
				{
					temp=(((float)adczu[k]*(3.3/4096)-1.5)*10/3);
					lBufInArray[k] = ((signed short)(temp*1000)) << 16;
				}

				cr4_fft_1024_stm32(lBufOutArray,lBufInArray,NPT);    
				GetPowerMag();

				fft_max=lBufMagArray[1];
				
				
				for(j=1;j<NPT/2;j++)
				{
					temp_max=lBufInArray[j];
					if(lBufMagArray[j]>=fft_max)
					{
						fft_max2 = fft_max;
						fft_max = lBufMagArray[j];
						fft_num=j*39.0625;
					}
				}			
					
			 if(fft_max > 2*fft_max2)
			 {
				 filter = fft_num;
			 }
			 
			 for(k=1;k<24;k++)
			 {
				 if(filter>filter_list[k-1]){filter_num = k;}
			 }
			 
			 ReadValue = filter_num;
			 GPIO_Write(GPIOB,filter_num);		
			 //printf("%d\r\n",ReadValue);
			}
		i += 1;
		}
}



