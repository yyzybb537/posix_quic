#pragma once

#include <gperftools/profiler.h>
#include <signal.h>

class GProfiler
{
public:
    static void Initialize() {
        signal(SIGTTIN, &SignalHandler);
        signal(SIGTTOU, &SignalHandler);
    }

    static void SignalHandler(int sig) {
        static bool profiling = false;

        switch (sig) {
            case SIGTTIN:
                if (!profiling) {
                    profiling = true;
                    printf("ProfilerStart\n");
                    ProfilerStart("/tmp/prof");
                }
                break;

            case SIGTTOU:
                if (profiling) {
                    profiling = false;
                    printf("ProfilerStop\n");
                    ProfilerStop();
                }
                break;
        }
    }
};

