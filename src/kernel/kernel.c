void kernel_main(void) {
    char* video = (char*) 0x98000;

    // clear the screen first
    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x0F;
    }

    const char* msg = "Hello from C kernel!";
    for (int i = 0; msg[i] != '\0'; i++) {
        video[i * 2] = msg[i];
        video[i * 2 + 1] = 0x0F;
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}