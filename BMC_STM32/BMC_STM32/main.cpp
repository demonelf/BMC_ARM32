#include "main.h"

uint32_t _clocks_in_us;

void delay_us(unsigned int delay)
{
	unsigned int t = delay * _clocks_in_us;
	for (unsigned int i = 0; i < t; i++)
	{
		__NOP();
	}
}



void blink()
{
	GPIO_SetBits(GPIOA, GPIO_Pin_5);
	delay_us(10000);
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
}


/*
 * stage|| 0 | 1 | 2 | 3 | 4 | 5 |  lo /  hi / fb
 * -----++---+---+---+---+---+---+
 *    U || H | H | Z | L | L | Z | PC0 / PC3 / PA0
 *    V || L | Z | H | H | Z | L | PC1 / PC4 / PA4
 *    W || Z | L | L | Z | H | H | PC2 / PC5 / PA5
 *
 *    H ||PC3|PC3|PC4|PC4|PC5|PC5|
 *    L ||PC1|PC2|PC2|PC0|PC0|PC1|
 */

static const uint8_t PhasePerRev = 6;

static const uint16_t DrivePhasePattern_All = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;

static const uint16_t DrivePhasePatterns[PhasePerRev] =
{
	GPIO_Pin_1 | GPIO_Pin_3,
	GPIO_Pin_2 | GPIO_Pin_3,
	GPIO_Pin_2 | GPIO_Pin_4,
	GPIO_Pin_0 | GPIO_Pin_4,
	GPIO_Pin_0 | GPIO_Pin_5,
	GPIO_Pin_1 | GPIO_Pin_5,
};

static const uint16_t DrivePhasePatternsXored[PhasePerRev] =
{
	DrivePhasePattern_All ^ DrivePhasePatterns[0],
	DrivePhasePattern_All ^ DrivePhasePatterns[1],
	DrivePhasePattern_All ^ DrivePhasePatterns[2],
	DrivePhasePattern_All ^ DrivePhasePatterns[3],
	DrivePhasePattern_All ^ DrivePhasePatterns[4],
	DrivePhasePattern_All ^ DrivePhasePatterns[5],
};

static const uint8_t initRevCount = 100;

static const uint16_t initRevTime[initRevCount] = 
{
	550,
	500,
	450,
	400,
	350,
	300,
	250,
	200,
	150,
	100,
};

// The value must be kept in a range of 0 to 5.
static uint8_t DrivePhase = 0;

void StartMotor(void)
{
	printf("Entering hard commutating stage.\n");

	NVIC_DisableIRQ(COMP1_2_3_IRQn);
	NVIC_DisableIRQ(TIM4_IRQn);

	DrivePhase = 0;

	for (int i = 0; i < initRevCount; i++)
	{
		for (int j = 0; j < PhasePerRev; j++)
		{
			// Init position
			GPIO_ResetBits(GPIOC, DrivePhasePattern_All ^ DrivePhasePatterns[j]);
			GPIO_SetBits(GPIOC, DrivePhasePatterns[j]);
			//delay_us(initRevTime[i]);
			delay_us(200);
		}
	}

	GPIO_SetBits(GPIOB, GPIO_Pin_0);

	printf("sc\n");

	/* 
	 * COMP1���荞�݂́CCOMP1�o�̗͂����オ��G�b�W�ł̂ݔ�������D
	 * ���_�d�ʂ�COMP1�̔񔽓]���͂ɐڑ������̂ŁCCOMP1�̏o�͋ɐ��ݒ肪�񔽓]�i����j�̂Ƃ��C
	 * ����s���̓d�ʂ����_�d�ʂ𒴂���u�Ԃ�COMP1�̏o�͂͗���������D
	 * �]���āC����s���̓d�ʂ����_�d�ʂ�荂���Ȃ�u�Ԃ𑨂���ɂ�COMP1�̏o�͋ɐ��ݒ�𔽓]�ɂ���΂悭�C
	 * �t�ɁC����s���̓d�ʂ����_�d�ʂ��Ⴍ�Ȃ�u�Ԃ𑨂���ɂ�COMP1�̏o�͋ɐ��ݒ��񔽓]�ɂ���΂悢�D
	 */

	/*
	 * Phase5�ɂ����ẮCU���iPA0�j�̗����オ��G�b�W�����҂���D����āC�o�͋ɐ��͔��]�D
	 */
	PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA0, PeripheralInit::CompOutPol::Inverted);

	TIM_SetCounter(TIM4, 0);

	NVIC_EnableIRQ(COMP1_2_3_IRQn);
}

void StopMotor(void)
{

}

inline void Commutate()
{
	// 5���傫���������O���o���Ă��ǂ���������Ȃ��D
	if (DrivePhase >= 5)
	{
		DrivePhase = 0;
	}
	else
	{
		DrivePhase++;
	}

	GPIO_ResetBits(GPIOC, DrivePhasePatternsXored[DrivePhase]);
	GPIO_SetBits(GPIOC, DrivePhasePatterns[DrivePhase]);
}

void COMP1Interrupt(void)
{
	printf("c1i\n");
	NVIC_DisableIRQ(COMP1_2_3_IRQn);

	// TODO: �����ŃJ�E���^�̒l�𑀍삵�đ��x�𐧌䂷��
	TIM_CounterModeConfig(TIM4, TIM_CounterMode_Down);

	NVIC_EnableIRQ(TIM4_IRQn);
}

/*
 * ���Čf��
 * stage|| 0 | 1 | 2 | 3 | 4 | 5 |  lo /  hi / fb
 * -----++---+---+---+---+---+---+
 *    U || H | H | Z | L | L | Z | PC0 / PC3 / PA0
 *    V || L | Z | H | H | Z | L | PC1 / PC4 / PA4
 *    W || Z | L | L | Z | H | H | PC2 / PC5 / PA5
 *
 *    H ||PC3|PC3|PC4|PC4|PC5|PC5|
 *    L ||PC1|PC2|PC2|PC0|PC0|PC1|
 */


void TIM4Interrupt(void)
{
	printf("t4i\n");
	NVIC_DisableIRQ(TIM4_IRQn);

	// ������DrivePhase���X�V�����
	Commutate();

	switch (DrivePhase)
	{
	case 0:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA5, PeripheralInit::CompOutPol::NonInverted);
		break;
	case 1:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA4, PeripheralInit::CompOutPol::Inverted);
		break;
	case 2:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA0, PeripheralInit::CompOutPol::NonInverted);
		break;
	case 3:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA5, PeripheralInit::CompOutPol::Inverted);
		break;
	case 4:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA4, PeripheralInit::CompOutPol::NonInverted);
		break;
	case 5:
		PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA0, PeripheralInit::CompOutPol::Inverted);
		break;
	default:
		printf("drivephase outta range");
		// ��O�𑗏o���Ă��悢�D
		break;
	}

	/*
	* Phase0,2,4�ɂ����ẮC�e���̗���������G�b�W�����҂���D����āC�o�͋ɐ��͔񔽓]�D
	*/
	//PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA5, PeripheralInit::CompOutPol::NonInverted);

	TIM_CounterModeConfig(TIM4, TIM_CounterMode_Up);
	TIM_SetCounter(TIM4, 0);

	NVIC_EnableIRQ(COMP1_2_3_IRQn);
}

int main(void)
{
	//SystemInit();

	_clocks_in_us = SystemCoreClock / 1000000;

	PeripheralInit::InitGPIO();
	PeripheralInit::InitEXTI();

	//NVIC_SetPriority(COMP1_2_3_IRQn, 0x0F);
	//NVIC_EnableIRQ(COMP1_2_3_IRQn);
	NVIC_SetPriority(EXTI0_IRQn, 0x0F);
	NVIC_EnableIRQ(EXTI15_10_IRQn);

	PeripheralInit::InitTIM1();
	PeripheralInit::InitTIM2();
	PeripheralInit::InitTIM4();

	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

	PeripheralInit::InitCOMP1(PeripheralInit::Comp1InvInp::PA0);
	PeripheralInit::EnableCOMP1();

	blink();
	printf("Initialization completed.\n");

	const uint32_t time = 100;

	while (1)
	{
	}

	/*
	* stage|| 1 | 2 | 3 | 4 | 5 | 6 |  lo /  hi
	* -----++---+---+---+---+---+---+
	*    U || H | H | Z | L | L | Z | PC0 / PC3
	*    V || L | Z | H | H | Z | L | PC1 / PC4
	*    W || Z | L | L | Z | H | H | PC2 / PC5
	* 
	*    H ||PC3|PC3|PC4|PC4|PC5|PC5|
	*    L ||PC1|PC2|PC2|PC0|PC0|PC1|
	*/

	// init position
	GPIO_ResetBits(GPIOB, GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5);
	GPIO_SetBits(GPIOB, GPIO_Pin_4);
	TIM_SetCompare3(TIM3, 0);
	TIM_SetCompare1(TIM3, 500);

	while (1)
	{
		// 1
		//GPIO_ResetBits(GPIOB, GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5);
		//GPIO_SetBits(GPIOB, GPIO_Pin_4);
		TIM_SetCompare3(TIM3, 0);
		TIM_SetCompare1(TIM3, 500);

		//delay_us(500000);
	}

	return 0;
}
