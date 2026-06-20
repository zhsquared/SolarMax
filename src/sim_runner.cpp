#ifdef NATIVE_SIM
// Entry point for native Mac simulation.
// Delegates to the Arduino-style setup()/loop() in main.cpp.

void setup();
void loop();

int main() {
    setup();
    for (;;) loop();
}
#endif
