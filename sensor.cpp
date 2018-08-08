//  Common code for all sensors.
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <cocoos-cpp.h>  //  TODO: Workaround for cocoOS in C++
#include "sensor.h"
#include "display.h"

void sensor_task(void) {
  //  Background task to receive and process sensor data.
  //  This task will be reused by all sensors: temperature, humidity, altitude.
  //  Don't declare any static variables inside here because they will conflict
  //  with other sensors.
  SensorTaskData *taskData = NULL;  //  Declared outside the task to prevent cross-initialisation error in C++.
  uint8_t sensorDataCount;
  MsgQ_t queue; Evt_t event;  //  TODO: Workaround for msg_post() in C++.
  task_open();  //  Start of the task. Must be matched with task_close().
  for (;;) {  //  Run the sensor processing code forever. So the task never ends.
    //  We should not make this variable static, because the task data should be unique for each task.
    taskData = (SensorTaskData *) task_get_data();

    //  This code is executed by multiple sensors. We use a global semaphore to prevent 
    //  concurrent access to the single shared I2C Bus on Arduino Uno.
    debug(taskData->sensor->info.name, " >> Wait for semaphore"); ////
    sem_wait(i2cSemaphore);  //  Wait until no other sensor is using the I2C Bus. Then lock the semaphore.
    debug(taskData->sensor->info.name, " >> Got semaphore"); ////

    //  We have to fetch the data pointer again after the wait.
    taskData = (SensorTaskData *) task_get_data();

    //  Do we have new data?
    if (taskData->sensor->info.poll_sensor_func() > 0) {
      //  If we have new data, copy sensor data to task data.
      sensorDataCount = taskData->sensor->info.
        receive_sensor_data_func(taskData->data, sensorDataSize);
      taskData->count = sensorDataCount;  //  Number of float values returned.

      //  Copy sensor data into a display message.
      DisplayMsg msg;
      msg.super.signal = taskData->sensor->info.id;  //  id is either TEMP_DATA or GYRO_DATA.
      memset(msg.name, 0, sensorNameSize + 1);  //  Zero the name array.
      strncpy(msg.name, taskData->sensor->info.name, sensorNameSize);  //  Set the sensor name e.g. tmp
      msg.count = taskData->count;  //  Number of floats returned as sensor data.
      for (int i = 0; i < msg.count && i < sensorDataSize; i++) {
        msg.data[i] = taskData->data[i];
      }

      //  Send the message. Note: When posting a message, its contents are cloned into the message queue.
      debug(msg.name, " >> Send msg"); ////
      msg_post(taskData->display_task_id, msg);
    }

    //  We are done with the I2C Bus.  Release the semaphore so that another task can fetch the sensor data.
    debug(taskData->sensor->info.name, " >> Release semaphore"); ////
    sem_signal(i2cSemaphore);

    //  Wait a short while before polling the sensor again.
    debug(taskData->sensor->info.name, " >> Wait interval"); ////
    task_wait(taskData->sensor->info.poll_interval);
  }
  debug("task_close", 0); ////
  task_close();  //  End of the task. Should never come here.
}