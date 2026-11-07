#include<stdio.h>
#include<arduino.h>
#include<math.h>
#def time 
int min ,sec = 0,hr;
main()
{
  for(int i=0;i<10;i++)
    {
      hr = min * 60;
      min = sec * 60;
      sec += 0;
    }
  return 0;
}
