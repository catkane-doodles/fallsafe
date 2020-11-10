#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <wiringPi.h>
#include <time.h>
#include <poll.h>

#include <sys/time.h>

#include "common/queue.h"
#include "common/vector3.h"

#include "lib/sensehat/sensehat.h"

#define DEBUG

/* Psuedo code:
 *
 * Check timer thingy
 * If timer triggers, read data
    * If queue is full, dequeue
    * Then, queue data 
 * If queue is pull, push data into model, get result
 * If result is fall, wait for user input, blink red 
 * Else, continue running
 */

// Configurations
// TODO: allow CLI args to overide these
#define GatheringFrequency 30
static bool continueProgram = true;
static double dataGatherIntervalMS = 1000.0 / GatheringFrequency;
static unsigned int updateIntervalMicroSeconds = 1111;
static size_t queueTarget = 30;

typedef enum fallsafe_state
{
    INITIAL,
    NORMAL,
    FALLEN
} FallsafeState;

typedef struct fallsafe_context
{
    double applicationStartTime;
    double deltaTime;
    double unixTime;
    double actualInterval;
    Queue acceleroDataset;
    int joystickFB;
    struct pollfd evpoll;
    int sensehatfbfd;
    uint16_t *sensehatLEDMap;
    FallsafeState state;
} FallsafeContext;

/**
 * Read data from accelerometer and push to dataset queue 
*/
static Vector3 gather_data(const FallsafeContext *context)
{
    Vector3 acceleroData;
    shGet2GAccel(&acceleroData);
    queue_enqueue(context->acceleroDataset, &acceleroData);

#if defined(DEBUG)
    printf("[Gather Data] unixtime: %0.0lf actualinterval: %0.3lf queue: %zu accelerometer: ", context->unixTime, context->actualInterval, context->acceleroDataset->length);
    vector3_print(&acceleroData);
#endif // DEBUG

    return acceleroData;
}

static ActivityState process_data(ArrayList accelero_datachunk)
{
    // ML processing

    return rand() < RAND_MAX / 120 ? FALLING : STATIONARY;
}

static void send_thingsboardAccel(Vector3 data, double time_ms)
{
    // Send data to thingsboard
}

static void send_thingsboardState(ActivityState state, double time_ms)
{
    // Send data to thingsboard
}

static void perform_task(FallsafeContext *context)
{
    static ActivityState previousState;
    // Gather the sensor data at current moment instant
    Vector3 acceleroData = gather_data(context);

    send_thingsboardAccel(acceleroData, context->unixTime);

    // Check if queue is at the target length for processing
    if (context->acceleroDataset->length < queueTarget)
    {
        context->state = INITIAL;
        return;
    }
    context->state = NORMAL;

    // Dequeue buffer
    ArrayList dataChunk = arraylist_new(Vector3, queueTarget * 1.25);
    Vector3 data;
    queue_dequeue(context->acceleroDataset, &data);
    arraylist_push(dataChunk, &data);
    // Pass the deqeueued chunk to ML and get the activity state
    ActivityState state = process_data(dataChunk);
    send_thingsboardState(state, context->unixTime);
    arraylist_destroy(dataChunk);
    // Show LED
    if (previousState != state)
    {
        setMap(0x0000, context->sensehatLEDMap, &context->sensehatfbfd);
        previousState = state;

        if (state == FALLING)
        {
            context->state = FALLEN;
            printf("[Waiting Input] unixtime: %0.0lf\n", context->unixTime);
        }
    }
    drawActivity(state, context->sensehatLEDMap, &context->sensehatfbfd);
}

/**
 * To runs every update to check if it is time to gather data and process them
*/
static void check_perform_task(FallsafeContext *context)
{
    static double timepassed = 0;
    static double previousTime = 0;
    timepassed += context->deltaTime;
    if (timepassed >= dataGatherIntervalMS)
    {
        previousTime = previousTime == 0 ? context->applicationStartTime : previousTime;
        context->actualInterval = context->unixTime - previousTime;
        previousTime = context->unixTime;
        perform_task(context);
        timepassed -= dataGatherIntervalMS;
        if (timepassed > dataGatherIntervalMS)
        {
            timepassed = 0;
        }
    }
}

/**
 * If falling is detected, program state will changed to waiting user input to indicate whether it is an false positive.
 * If no input is detected after a certain period of time, an alert to external output will be triggered
*/
static void await_userinput(FallsafeContext *context)
{
    static Joystick joystick;

    while (poll(&context->evpoll, 1, 0) > 0)
    {
        if (readJoystick(&context->joystickFB, &joystick))
        {
            if (joystick.state == PRESSED)
            {
                printf("[INFO] Joystick PRESSED direction: %s=====================================================\n", joystick.direction);
            }
            if (joystick.state == HOLD)
            {
                printf("[INFO] Joystick HOLD direction: %s=====================================================\n", joystick.direction);
            }
            if (joystick.state == RELEASE)
            {
                printf("[INFO] Joystick RELEASE direction: %s=====================================================\n", joystick.direction);
                context->state = INITIAL;
                queue_destroy(context->acceleroDataset);
                context->acceleroDataset = queue_new(Vector3, queueTarget);
                setMap(0x0000, context->sensehatLEDMap, &context->sensehatfbfd);
            }
        }
        else
        {
            fprintf(stderr, "[ERROR] Joystick read error. \n");
        }
    }
}

static void update_rolling_led(FallsafeContext *context)
{
    static const double interval = 1000.0 / 30;
    static int previousPosition = 0;
    static int currentPosition = 0;
    static double timepassed = 0;
    timepassed += context->deltaTime;
    if (timepassed > interval)
    {
        shSetPixel(previousPosition, 0, 0, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(currentPosition, 0, 0x4000, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(7, previousPosition, 0, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(7, currentPosition, 0x4000, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(0, 7 - previousPosition, 0, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(0, 7 - currentPosition, 0x4000, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(7 - previousPosition, 7, 0, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        shSetPixel(7 - currentPosition, 7, 0x4000, 1, context->sensehatLEDMap, &context->sensehatfbfd);
        previousPosition = currentPosition;
        currentPosition = (currentPosition + 1) % 7;
        timepassed -= interval;
        if (timepassed > interval)
        {
            timepassed = 0;
        }
    }
}

/**
 * Main update loop
*/
static void update(FallsafeContext *context)
{
    update_rolling_led(context);
    switch (context->state)
    {
    case INITIAL:
    case NORMAL:
        check_perform_task(context);
        break;
    case FALLEN:
        await_userinput(context);
        break;
    default:
        fprintf(stderr, "[ERROR] Invalid State: %d\n", context->state);
        break;
    }
}

/**
 * Gets unix time offset in milliseconds
*/
static double get_unixtime_ms()
{
    static struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * 1000.0 + currentTime.tv_usec / 1000.0;
}

/**
 * Gets high precision arbitary time in milliseconds
*/
static double get_monotonicclock_ms(void)
{
    static struct timespec _ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &_ts);
    return _ts.tv_sec * 1000.0 + _ts.tv_nsec / 1000000.0;
}

/**
 * Handles CTRL+C to gracefully exit program
*/
void exit_handler(int signum)
{
    printf("\n[INFO] Handling SIGINT: %d\n", signum);
    continueProgram = false;
}

int main(int agc, char **argv)
{
    FallsafeContext context;
    double previousTime;
    double currentTime;

    context.acceleroDataset = queue_new(Vector3, queueTarget * 1.25);
    context.state = INITIAL;
    context.evpoll.events = POLLIN;

    // Set up programming termination handler
    signal(SIGINT, exit_handler);
    // Set up wiring pi
    wiringPiSetupSys();

    // Set up sensehat sensors
    if (shInit(1, &context.sensehatfbfd) == 0)
    {
        fprintf(stderr, "[ERROR] Unable to open sense, is it connected?\n");
        return -1;
    }
    if (mapLEDFrameBuffer(&context.sensehatLEDMap, &context.sensehatfbfd) == 0)
    {
        fprintf(stderr, "[ERROR] Unable to map LED to Frame Buffer. \n");
        return -1;
    }
    if (initJoystick(&context.joystickFB) == -1)
    {
        fprintf(stderr, "[ERROR] Unable to open joystick event\n");
        return -1;
    }

    context.evpoll.fd = context.joystickFB;
    context.applicationStartTime = get_unixtime_ms();
    previousTime = get_monotonicclock_ms();

    while (continueProgram)
    {
        delayMicroseconds(updateIntervalMicroSeconds);
        currentTime = get_monotonicclock_ms();
        context.deltaTime = currentTime - previousTime;
        previousTime = currentTime;
        context.unixTime = get_unixtime_ms();
        update(&context);
    }

    queue_destroy(context.acceleroDataset);
    setMap(0x0000, context.sensehatLEDMap, &context.sensehatfbfd);
    shShutdown(&context.sensehatfbfd, context.sensehatLEDMap);
    printf("[INFO] Program terminated\n");
    return 0;
}
