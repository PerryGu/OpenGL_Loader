#include "application.h"

int main() {
    // Application object uses RAII - destructor handles cleanup automatically
    Application app;
    if (app.init()) {
        app.run();
    }
    // No explicit cleanup() call needed - Application destructor handles it
    return 0;
}
