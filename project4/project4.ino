#include "scheduler.h"
#define LOADFACTOR 8
#define TASK1DELAY 9.4*LOADFACTOR
#define TASK2DELAY 9.9*LOADFACTOR
#define TASK3DELAY 19.9*LOADFACTOR
TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;

// the loop function runs over and over again forever
void loop() {}


static void testFunc1( void *pvParameters ){
int i, a;
Serial.println("#Executing T1");
  for( i = 0; i < 100; i++ )
  {
    a = 1 + i*i*i*i;
  }
  delay(TASK1DELAY);
}


int stopc = 0;
static void testFunc2( void *pvParameters )
{

int i, a;
Serial.println("#Executing T2");
  for( i = 0; i < 100; i++ )
  {
    a = 1 + i*i*i*i;
  }
  delay(TASK2DELAY);
}

static void testFunc3( void *pvParameters ){
  int i, a;
  Serial.println("#Executing T3");
  for(i = 0; i < 100; i++ )
  {
    a = 1 + a * a * i;
  }
  delay(TASK3DELAY);
}


static void testFuncA1( void *pvParameters )
{
char* c = pvParameters;
int i;
  Serial.println("#Executing A1");
  for( i=0; i < 1000; i++ )
  {
  int a = i * i * i * i;
  }
  delay(30);
 }

static void testFuncA2( void *pvParameters)
{
char* c = pvParameters;
int i;
  Serial.println("#Executing A2");

  for( i=0; i < 10000; i++ )
  {
  int a = i * i * i * i;
  }
  delay(30);
  }

static void testFuncA3( void *pvParameters)
{
char* c = pvParameters;
int i;
  Serial.println("#Executing A3");

  for( i=0; i < 1000; i++ )
  {
  int a = i * i * i * i;
  }
  delay(30);
}

static void testFuncA4( void *pvParameters )
{
char* c = pvParameters;
int i;

  Serial.println("#Executing A4");

  for( i=0; i < 1000; i++ )
  {
  int a = i * i * i * i;
  }
  delay(30);
}

int main( void )
{
  init();
  Serial.begin(19200);
  
  char c1 = 'a';
  char c2 = 'b';    
  char c3 = 'c';
  char c4 = 'd';
 
  vSchedulerInit(); 

  //TaskSet 1.1
//  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(50), pdMS_TO_TICKS(150));
//  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS(150), pdMS_TO_TICKS(100));
  /* Args:                     FunctionName, Name, StackDepth, *pvParameters, Priority, TaskHandle, Phase,         Period,             MaxExecTime,       Deadline */
//TaskSet 1
  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS(200), pdMS_TO_TICKS(200));
  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), pdMS_TO_TICKS(500));
  vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, NULL, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(600), pdMS_TO_TICKS(600), pdMS_TO_TICKS(600));
 // vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, NULL, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(5000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(5000));
  
   // delay(200);

//TaskSet 2
//  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(100), pdMS_TO_TICKS(400));
//  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(150), pdMS_TO_TICKS(200));
//  vSchedulerPeriodicTaskCreate(testFunc3, "t3", configMINIMAL_STACK_SIZE, &c3, NULL, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(500), pdMS_TO_TICKS(200), pdMS_TO_TICKS(700));
//  vSchedulerPeriodicTaskCreate(testFunc4, "t4", configMINIMAL_STACK_SIZE, &c4, NULL, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1000));

  vSchedulerAperiodicTaskCreate( testFuncA1, "A1", "A1-1", pdMS_TO_TICKS( 50 ) );
  vSchedulerAperiodicTaskCreate( testFuncA2, "A2", "A2-1", pdMS_TO_TICKS( 50 ) );
  vSchedulerAperiodicTaskCreate( testFuncA3, "A3", "A3-1", pdMS_TO_TICKS( 50 ) );
  vSchedulerAperiodicTaskCreate( testFuncA4, "A4", "A4-1", pdMS_TO_TICKS( 50 ) );
  
  vSchedulerStartInit();
  Serial.println("TESTING ====================>");
  vSchedulerStart();
  /* If all is well, the scheduler will now be running, and the following line
  will never be reached. */
  
  for( ;; );
}
