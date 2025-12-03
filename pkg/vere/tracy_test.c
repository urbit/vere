#include <stdio.h>
#include <unistd.h>

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#endif

void test_function() {
#ifdef TRACY_ENABLE
    TracyCZone(ctx, 1);
    TracyCZoneName(ctx, "test_function", 13);
#endif
    
    printf("Testing Tracy integration - function called\n");
    
#ifdef TRACY_ENABLE
    TracyCMessageL("Test message from urbit");
#endif
    
    // Simulate some work
    for (int i = 0; i < 1000000; i++) {
        // Some CPU work for Tracy to profile
    }
    
#ifdef TRACY_ENABLE
    TracyCZoneEnd(ctx);
#endif
}

int main() {
    printf("Tracy test starting - connect Tracy profiler to localhost:8086\n");
    printf("Application will run for 10 seconds...\n");
    
    for (int frame = 0; frame < 100; frame++) {
#ifdef TRACY_ENABLE
        TracyCFrameMark;
#endif
        test_function();
        
        // Sleep for 100ms to make frames visible
        usleep(100000);
        
        if (frame % 10 == 0) {
            printf("Frame %d/100\n", frame);
        }
    }
    
    printf("Tracy integration test completed\n");
    return 0;
}
