#include "controller.h"
#include "microphone.h"

#include <iostream>
#include <fstream>

int main(int argc, char**argv) {
    (void) argc;
    (void)argv;

    /*Microphone mic;
    std::ofstream out("mic.bin", std::ios_base::binary);
    mic.Start();
    int16_t buf[4410];
    for(int n=0; n<100; n++) {
        mic.Read(buf, 4410);
        out.write(reinterpret_cast<char*>(buf), sizeof(buf));
    }
    out.close();

    return 1;*/

    Controller c;
    return c.Run();
}
