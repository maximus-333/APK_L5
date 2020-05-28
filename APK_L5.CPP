#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>

struct timeStruct{
	int sec;
	int min;
	int hour;
	int dayWeek;
	int dayMonth;
	int month;
	int year;
};

//waits for register access and stops RTC
//If timer doesn't open up in 3 seconds, returns false
void cmosLock()
{
   char lockRes = 1;
   int waitCycles = 3000;
   do
   {
      outp(0x70, 0xA);	//0xA - state register A
      lockRes = inp(0x71) & 0x80;//if highest bit is '0', registers ready
      delay(1);
      if(waitCycles-- == 0)
      {
	 lockRes = 0;
      }

   }
   while(lockRes);

   //turn off clock updates
   outp(0x70, 0xB);       //0xB - state register B
   outp(0x71, inp(0x71)|0x80);
}

//Starts the timer
void cmosUnlock()
{
   outp(0x70, 0xB);
   outp(0x71, inp(0x71) & 0x7f);
}



//get system time
void opt1()
{
	timeStruct currTime;

	//Lock timer, get time, unlock
	cmosLock();

	//Read the time
	outp(0x70, 4);	//hours
	currTime.hour = inp(0x71);
	outp(0x70, 2);	//minutes
	currTime.min = inp(0x71);
	outp(0x70, 0);  //seconds
	currTime.sec = inp(0x71);
	//Now, unlock timer updates
	cmosUnlock();

	printf("Current time: %d:%d:%d\n", currTime.hour, currTime.min, currTime.sec);
}

void opt2()
{
	timeStruct newTime;

	printf("Input new time - HH MM SS:");

	scanf("%d %d %d", &newTime.hour, &newTime.min, &newTime.sec);
	//got data. Lock timer, put new time, unlock
	cmosLock();

	//Read/write the time
	outp(0x70, 4);	//hours
	outp(0x71, newTime.hour);
	outp(0x70, 2);	//minutes
	outp(0x71, newTime.min);
	outp(0x70, 0);	//seconds
	outp(0x71, newTime.sec);
	//Now, unlock timer updates
	cmosUnlock();
}


int counterDelay;
void interrupt (*oldDelayInt)(...);
void interrupt newDelayInt(...)
{
   counterDelay--;
   oldDelayInt();
}

//Delay, precise to millisecond
void opt3()
{
	int delayTempVal;
	//User input and such
	printf("Input delay in ms:");
	scanf("%d", &delayTempVal);


	//---Technical part---

	_disable();

	oldDelayInt = getvect(0x70);
	setvect(0x70, newDelayInt);

	_enable();

	//Unmask IRQ8 interrupt
	//(IRQ8 is on slave, and its mask register is on address 0xA1)
	outp(0xA1, (inp(0xA1) & 0xFE));

	//activate periodic interrupt mode on CMOS
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) | 0x40);	//set periodic mode

	counterDelay = delayTempVal;

	printf("Wait begin \n");


	//Just wait untill the delay completes
	while(counterDelay >= 0);

	printf("Wait end \n");


	_disable();
	//Finished waiting. Deactivate periodic interrupts
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) & 0xBF);

	setvect(0x70, oldDelayInt);

	_enable();
}


void interrupt (*oldAlarmInt)(...);
void interrupt newAlarmInt(...)
{
   short far *screen = (short far*)MK_FP(0xB800, 0);

   char alarmStr[] = "Alarm triggered!";

   int i;
   for(i = 0;i<strlen(alarmStr);i++)
   {
      *screen = 0x1E00;
      *screen |= (char)alarmStr[i];
      screen ++;
   }

   outp(0x70, 0x0B);
   outp(0x71, inp(0x71) & 0xDF);
}


void opt4()
{
	//Algorythm:
	//Set alarm clock interrupt (IRQ8 - 0x70) to something
	//Pause the timer, like in opt1()
	//In CMOS's state reg B:unset bits 4 and 6
	    //(other interrupt options), set bit 5(alarm clock int)
	//Set alarm clock's HH-MM-SS(all in BCD!!)
	//Start the timer, and wait(maybe make resident???)
	//When done, reset interrupt and unset bit 5


	//User inputs alarm clock time
	timeStruct alarmTime;
	printf("Input alarm time - HH MM SS:");
	scanf("%d %d %d", &alarmTime.hour, &alarmTime.min, &alarmTime.sec);

	//Technical part
	_disable();

	//wait for timer
	cmosLock();

	//Set alarm clock values.
	//They're located in regs 5,3,1(HH,MM,SS)
	outp(0x70, 5);
	outp(0x71, alarmTime.hour);
	outp(0x70, 3);
	outp(0x71, alarmTime.min);
	outp(0x70, 1);
	outp(0x71, alarmTime.sec);

	//set alarm clock mode
	outp(0x70, 0xB);
	outp(0x71, inp(0x71) | 0x20);

	oldAlarmInt = getvect(0x4A);
	setvect(0x4a, newAlarmInt);

	cmosUnlock();

	outp(0xA1, inp(0xA1) & 0xFE);

	_enable();

	printf("ALARM SET \n");
}





int main()
{
	_disable();

	//Change CMOS time to pure binary(from BCD)
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) | 0x04);

	_enable();

	while(1)
	{
		int option;
		printf("Options: \n");
		printf("1 - get time \n");
		printf("2 - set time \n");
		printf("3 - make delay \n");
		printf("4 - set alarm\n");
		printf("Input option(1-4): ");
		scanf("%d", &option);

		switch(option)
		{
			case(1):
			{
				opt1();

				break;
			}
			case(2):
			{
				opt2();

				break;
			}
			case(3):
			{
				opt3();

				break;
			}
			case(4):
			{
				opt4();

				break;
			}

			default:
			{
				return 0;
			}
		}
	}
}