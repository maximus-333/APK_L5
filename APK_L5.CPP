#include <stdio.h>
#include <conio.h>
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





//get/set system time
void opt1()
{
	timeStruct currTime;
	timeStruct newTime;

	printf("Input new time - HH MM SS:");

	scanf("%d %d %d", &newTime.hour, &newTime.min, &newTime.sec);
	//got data. Lock timer, get time, put new time, unlock
	cmosLock();

	//Read/write the time
	outp(0x70, 4);	//hours
	currTime.hour = inp(0x71);
	outp(0x71, newTime.hour);
	outp(0x70, 2);	//minutes
	currTime.min = inp(0x71);
	outp(0x71, newTime.min);
	outp(0x70, 0);	//seconds
	currTime.sec = inp(0x71);
	outp(0x71, newTime.sec);
	//Now, unlock timer updates
	cmosUnlock();

	printf("Previous time: %d:%d:%d\n", currTime.hour, currTime.min, currTime.sec);


}

int delayValue = 0;
void interrupt (*defTimerInt)(...); //default interrupt for timer
void interrupt newTimerInt(...)     //new interrupt, with delay
{
   _disable();

   --delayValue;
   defTimerInt();

   _enable();
}


//Delay, precise to millisecond
void opt2()
{
	//User input and such
	printf("Input delay in ms:");
	scanf("%d", &delayValue);


	//---Technical part---

	_disable();

	defTimerInt = getvect(0x70);  //get old RTC interrupt vector(IRQ8)
	setvect(0x70, newTimerInt);   //set new vector, with delay added

	//activate periodic interrupt mode on CMOS
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) | 0x40);	//set periodic mode


	//prepared for interrupt. Delay time was set earlier.
	_enable();

	//Just wait untill the delay completes
	while(delayValue >= 0);

	_disable();
	//Finished waiting. Deactivate periodic interrupts
	//and reset IRQ8
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) & 0xBF);
	setvect(0x70, defTimerInt);

	_enable();
}


void interrupt (*alarmOld)(...);
void interrupt alarmNew(...)
{
   _disable();


   printf("ALARM TRIGGERED!\n");

   //alarmOld();

   printf("P1\n");

   //Reset all back to default
   cmosLock();

   printf("P2\n");

   //Unset alarm clock mode
   outp(0x70, 0x0B);
   outp(0x71, inp(0x71) & 0xDF);
   //Go back to default interrupt
   setvect(0x70, alarmOld);
   cmosUnlock();

   printf("P3\n");

   _enable();
}



void opt3()
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
	//Assign new handler
	alarmOld = getvect(0x70);
	setvect(0x70, alarmNew);

	cmosUnlock();

	_enable();

	printf("ALARM SET \n");
}





int main()
{
	_disable();

	//Initialize interrupt controllers(ICW1-ICW4)
	//master
   //	outp(0x20, 0x11);
   //	outp(0x21, 0x08);
   //	outp(0x21, 0x04);
   //	outp(0x21, 0x01);
	//slave
   //	outp(0xA0, 0x11);
   //	outp(0xA1, 0x70);
   //	outp(0xA1, 0x02);
   //	outp(0xA1, 0x01);

	//Unmask IRQ8 interrupt
	//(IRQ8 is on slave, and its mask register is on address 0xA1)
	outp(0xA1, (inp(0xA1) & 0xFE));

	//Change CMOS time to pure binary(from BCD)
	outp(0x70, 0x0B);
	outp(0x71, inp(0x71) | 0x04);


	_enable();

	while(1)
	{
		int option;
		printf("Input option(1-3): ");
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
			default:
			{
				return 0;
			}
		}
	}
}