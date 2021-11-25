#include "main.h"
#include "kpi.h"
#include <queue>
#include <cmath>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"
#include "msgbus.h"

using namespace std;

extern char* __progname;

#define SYNBENCH_ERROR_1_FMT      "synbench/%d/kpi/error/1"
#define SYNBENCH_ERROR_2_FMT      "synbench/%d/kpi/error/2"
#define SYNBENCH_WARNING_1_FMT    "synbench/%d/kpi/warning/1"
#define SYNBENCH_WARNING_2_FMT    "synbench/%d/kpi/warning/2"

#define SYNBENCH_STATUS_FMT       "synbench/%d/kpi/status"

static char gTopicError1[64]          = {0};
static char gTopicError2[64]          = {0};
static char gTopicWarning1[64]        = {0};
static char gTopicWarning2[64]        = {0};
static char gTopicStatus[32]          = {0};

static uint64_t gKPIMinFramesInWindow     = 0;       // calculated and used for KPI #2 (gKPIWindow / gKPIMinFPS)
static double   gKPIMTBFTime_Milliseconds = 0.0f;    // time in seconds, used for all KPIs measured

static MQTTAsync gClient;


void evaluate_kpis(struct config *conf, double frameTime_Milliseconds) {
    struct timeval             tv;
    double                     timeOfFailure_Milliseconds     = 0.0f;

    //
    // Static-global variables - therefore limited to this function's scope
    //
    static std::queue<double>  gKPIFrameTimeQueue;                       // queue of frame times in milliseconds
    static double              gKPIQueueDuration_Milliseconds = 0.0f;    // running total of aggregate frame times in the queue
    static uint64_t            gKPICurrentFrameCount          = 1;       // overall frame count, always increasing
    static uint64_t            gKPIPauseToFrame               = 0;
  
    //
    // Start measuring KPIs after settling time has concluded
    //
    if ((uint64_t)(conf->kpi_settling_time_seconds * conf->kpi_target_fps) < gKPICurrentFrameCount) {
        static double timeOfTestStart_Milliseconds = 0.0f;

        if (0 == timeOfTestStart_Milliseconds) {
            gettimeofday(&tv, NULL);
            timeOfTestStart_Milliseconds = (((double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f)) * 1000.0f);
            printf("Test Start Time (%0.2lf)\n", timeOfTestStart_Milliseconds);
        }

        if (gKPICurrentFrameCount >= gKPIPauseToFrame) {
            //
            // KPI DEADLINE #1 (SINGLE FRAME MAX DURATION)
            //
            if (frameTime_Milliseconds > conf->kpi_max_frametime_milliseconds) {
                gettimeofday(&tv, NULL);
                timeOfFailure_Milliseconds = (((double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f)) * 1000.0f);

                if ((timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds) < gKPIMTBFTime_Milliseconds) {
                    char error_message[1024];
                    snprintf(error_message, sizeof(error_message), "[Frame:%ld] ERROR-1: Workload rendered single frame (%0.02lf ms) slower than maximum (%0.2lf ms)", gKPICurrentFrameCount, frameTime_Milliseconds, conf->kpi_max_frametime_milliseconds);
                    send_msg(&gClient, error_message, gTopicError1); 
                    printf("%s\n", error_message);
                    printf("  |-- Failure Time: %0.2lf ms (MTBF = %0.2lf ms)\n", timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds, gKPIMTBFTime_Milliseconds);

                    // don't allow systemic errors to report at framerate.  pause kpi measurements for one window size
                    gKPIPauseToFrame = gKPICurrentFrameCount + gKPIMinFramesInWindow;

                    if (conf->kpi_exit_on_failure) {
                        exit_synbench(-1);
                    }
                } else {
                    char error_message[1024];
                    snprintf(error_message, sizeof(error_message), "[Frame:%ld] WARNING-1: Workload rendered single frame (%0.02lf ms) slower than maximum (%0.2lf ms)", gKPICurrentFrameCount, frameTime_Milliseconds, conf->kpi_max_frametime_milliseconds);
                    send_msg(&gClient, error_message, gTopicWarning1);
                    printf("%s\n", error_message);
                    printf("  |-- Failure Time: %0.2lf ms (MTBF = %0.2lf ms)\n", timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds, gKPIMTBFTime_Milliseconds);
                }

                timeOfTestStart_Milliseconds = timeOfFailure_Milliseconds;
            }

            //
            // KPI DEADLINE #2 (WINDOW FRAME COUNT - AVERAGE FRAME RATE)
            //
            if ((gKPIQueueDuration_Milliseconds + frameTime_Milliseconds) >= conf->kpi_window_milliseconds) {
                if ((uint64_t)gKPIFrameTimeQueue.size()< gKPIMinFramesInWindow) {
                    gettimeofday(&tv, NULL);
                    timeOfFailure_Milliseconds = (((double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f)) * 1000.0f);

                    if ((timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds) < gKPIMTBFTime_Milliseconds) {
                        char error_message[1024];
                        snprintf(error_message, sizeof(error_message), "[Frame:%ld] ERROR-2: Workload rendered %d (below min of %d) frames in %0.3lf milliseconds", gKPICurrentFrameCount, (int)gKPIFrameTimeQueue.size(), (int)gKPIMinFramesInWindow, gKPIQueueDuration_Milliseconds);
                        send_msg(&gClient, error_message, gTopicError2); 
                        printf("%s\n", error_message);
                        printf("  |-- Failure Time: %0.2lf ms (MTBF = %0.2lf ms)\n", timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds, gKPIMTBFTime_Milliseconds);

                        // don't allow systemic errors to report at framerate.  pause kpi measurements for one window size
                        gKPIPauseToFrame = gKPICurrentFrameCount + gKPIMinFramesInWindow;

                        if (conf->kpi_exit_on_failure) {
                            exit_synbench(-1);
                        }
                    } else {
                        char error_message[1024];
                        snprintf(error_message, sizeof(error_message), "[Frame:%ld] WARNING-2: Workload rendered %d (below min of %d) frames in %0.3lf milliseconds", gKPICurrentFrameCount, (int)gKPIFrameTimeQueue.size(), (int)gKPIMinFramesInWindow, gKPIQueueDuration_Milliseconds);
                        send_msg(&gClient, error_message, gTopicWarning2);
                        printf("%s\n", error_message);
                        printf("  |-- Failure Time: %0.2lf ms (MTBF = %0.2lf ms)\n", timeOfFailure_Milliseconds - timeOfTestStart_Milliseconds, gKPIMTBFTime_Milliseconds);
                    }

                    timeOfTestStart_Milliseconds = timeOfFailure_Milliseconds;
                }
            }
        }

        //
        // Get the queue back under KPI Window, so we are not already over time before the next frame arrives
        // TODO - is this correct?  or should we simply pop 1 frame each time we push 1 frame?
        //
        while ((gKPIQueueDuration_Milliseconds + frameTime_Milliseconds) >= conf->kpi_window_milliseconds) {
            gKPIQueueDuration_Milliseconds -= gKPIFrameTimeQueue.front();
            gKPIFrameTimeQueue.pop();
        }

        gKPIFrameTimeQueue.push(frameTime_Milliseconds);
        gKPIQueueDuration_Milliseconds += frameTime_Milliseconds;
    }
    
    gKPICurrentFrameCount++;
}

int init_kpi_model(struct config *conf) {
    int ret = MQTTASYNC_SUCCESS;
    int pid = getpid();

    snprintf(gTopicError1, sizeof(gTopicError1), SYNBENCH_ERROR_1_FMT, pid);
    snprintf(gTopicError2, sizeof(gTopicError2), SYNBENCH_ERROR_2_FMT, pid); 
    snprintf(gTopicWarning1, sizeof(gTopicWarning1), SYNBENCH_WARNING_1_FMT, pid);
    snprintf(gTopicWarning2, sizeof(gTopicWarning2), SYNBENCH_WARNING_2_FMT, pid);

    snprintf(gTopicStatus, sizeof(gTopicStatus), SYNBENCH_STATUS_FMT, pid);

    gKPIMinFramesInWindow = floor((conf->kpi_window_milliseconds / 1000.0) * (double)conf->kpi_min_fps);
    printf("gKPIMinFramesInWindow = %d\n", (int)gKPIMinFramesInWindow);

    gKPIMTBFTime_Milliseconds = 1000.0f * (pow(10.0f, (double)conf->kpi_number_nines) / (double)conf->kpi_target_fps);
    printf("gKPIMTBFTime = %0.2lf ms\n", gKPIMTBFTime_Milliseconds);

    if (gKPIMTBFTime_Milliseconds < conf->kpi_window_milliseconds) {
        printf("ERROR: KPI MTBF (%0.2lf s) configured (via # 9's) to smaller time window as compared kpi measurement time (%0.2lf s)\n", gKPIMTBFTime_Milliseconds, conf->kpi_window_milliseconds);
        exit_synbench(-1);
    }

    ret = init_msg_bus(&gClient);

    if (MQTTASYNC_SUCCESS == ret) {
        send_msg(&gClient, "connected", gTopicStatus);
    }

    return ret;
}

void cleanup_kpi_model() {
    send_msg(&gClient, "terminated", gTopicStatus);
    usleep(250000);
    cleanup_msg_bus(&gClient);
}
